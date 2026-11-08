#include "richtext.h"
#include <QApplication>
#include <QFontMetricsF>
#include <QComboBox>
#include <QHash>
#include <QLabel>

namespace {

const QHash<QChar, QChar> &superscripts()
{
    static const QHash<QChar, QChar> m = {
        {QChar(0x2070), '0'}, {QChar(0x00B9), '1'}, {QChar(0x00B2), '2'}, {QChar(0x00B3), '3'}, {QChar(0x2074), '4'},
        {QChar(0x2075), '5'}, {QChar(0x2076), '6'}, {QChar(0x2077), '7'}, {QChar(0x2078), '8'}, {QChar(0x2079), '9'},
        {QChar(0x207A), '+'}, {QChar(0x207B), QChar(0x2212)}, {QChar(0x207F), 'n'}, {QChar(0x02E3), 'x'},
        {QChar(0x1D4D), 'g'}, {QChar(0x02B3), 'r'}};
    return m;
}

const QHash<QChar, QChar> &subscripts()
{
    static const QHash<QChar, QChar> m = {
        {QChar(0x2080), '0'}, {QChar(0x2081), '1'}, {QChar(0x2082), '2'}, {QChar(0x2083), '3'}, {QChar(0x2084), '4'},
        {QChar(0x2085), '5'}, {QChar(0x2086), '6'}, {QChar(0x2087), '7'}, {QChar(0x2088), '8'}, {QChar(0x2089), '9'},
        {QChar(0x208B), QChar(0x2212)}, {QChar(0x2090), 'a'}, {QChar(0x2091), 'e'}, {QChar(0x2096), 'k'},
        {QChar(0x2099), 'n'}, {QChar(0x209A), 'p'}, {QChar(0x2C7C), 'j'}, {QChar(0x2098), 'm'}};
    return m;
}

struct Run {
    QString text;
    int level = 0;     // 1 = superscript, -1 = subscript
    bool box = false;  // template placeholder
};

QList<Run> parse(const QString &s)
{
    QList<Run> runs;
    auto add = [&](const QString &t, int level, bool box = false) {
        if (!box && !runs.isEmpty() && !runs.last().box && runs.last().level == level) runs.last().text += t;
        else runs.push_back({t, level, box});
    };
    int i = 0, level = 0, depth = 0;
    while (i < s.size()) {
        QChar c = s[i];
        if ((c == '^' || c == '_') && i + 1 < s.size() && s[i + 1] == '{' && depth == 0) {
            level = c == '^' ? 1 : -1;
            depth = 1;
            i += 2;
            continue;
        }
        if (c == '}' && depth > 0) {
            depth = 0;
            level = 0;
            ++i;
            continue;
        }
        if (c == '[' && i + 1 < s.size() && s[i + 1] == ']') {
            add({}, level, true);
            i += 2;
            continue;
        }
        add(QString(c), level);
        ++i;
    }
    return runs;
}

QFont scaled(const QFont &f, int level)
{
    if (level == 0) return f;
    QFont g = f;
    if (f.pixelSize() > 0) g.setPixelSize(std::max(5, int(std::round(f.pixelSize() * 0.66))));
    else g.setPointSizeF(std::max(4.0, f.pointSizeF() * 0.66));
    return g;
}

qreal runWidth(const Run &r, const QFont &f)
{
    QFontMetricsF fm(scaled(f, r.level));
    if (r.box) return fm.height() * 0.5 + 2;
    return fm.horizontalAdvance(r.text);
}

} // namespace

QString unicodeToMarkup(const QString &s)
{
    QString out;
    int mode = 0;   // 1 inside ^{, -1 inside _{
    auto close = [&] {
        if (mode) out += '}';
        mode = 0;
    };
    for (int i = 0; i < s.size(); ++i) {
        QChar c = s[i];
        if (c == '^' && mode == 0 && i + 1 < s.size() && s[i + 1] != '{') {   // linear exponent "10^−4"
            int j = i + 1;
            if (j < s.size() && (s[j] == '-' || s[j] == QChar(0x2212))) ++j;
            int k = j;
            while (k < s.size() && (s[k].isDigit() || s[k] == '.')) ++k;
            if (k > j) {
                out += "^{" + s.mid(i + 1, k - i - 1) + "}";
                i = k - 1;
                continue;
            }
        }
        if (c == QChar(0x221B)) {   // ∛ → ³√
            close();
            out += "^{3}√";
            continue;
        }
        if (c == QChar(0x2610)) {   // ☐
            if (mode == 0 && !out.isEmpty() && (out.back().isLetterOrNumber() || out.back() == ')') &&
                (i + 1 >= s.size() || !s[i + 1].isLetterOrNumber()) && !out.endsWith("√"))
                out += "^{[]}";   // x☐, 10☐, e☐ are power templates
            else {
                close();
                out += "[]";
            }
            continue;
        }
        int want = superscripts().contains(c) ? 1 : subscripts().contains(c) ? -1 : 0;
        if (want != mode) {
            close();
            if (want) out += want == 1 ? "^{" : "_{";
            mode = want;
        }
        if (want == 1) out += superscripts().value(c);
        else if (want == -1) out += subscripts().value(c);
        else out += c;
    }
    close();
    return out;
}

qreal richWidth(const QString &markup, const QFont &f)
{
    qreal w = 0;
    for (const Run &r : parse(markup)) w += runWidth(r, f);
    return w;
}

void drawRich(QPainter &p, const QRectF &rect, const QString &markup, const QFont &f, Qt::Alignment align)
{
    const QList<Run> runs = parse(markup);
    qreal total = 0;
    for (const Run &r : runs) total += runWidth(r, f);
    QFontMetricsF fm(f);
    qreal x = rect.left();
    if (align & Qt::AlignHCenter) x = rect.center().x() - total / 2;
    else if (align & Qt::AlignRight) x = rect.right() - total;
    qreal base;
    if (align & Qt::AlignTop) base = rect.top() + fm.ascent();
    else if (align & Qt::AlignBottom) base = rect.bottom() - fm.descent();
    else base = rect.center().y() + (fm.ascent() - fm.descent()) / 2;

    p.save();
    const QColor ink = p.pen().color();
    for (const Run &r : runs) {
        QFont rf = scaled(f, r.level);
        QFontMetricsF rfm(rf);
        qreal y = base;
        if (r.level == 1) y = base - fm.ascent() * 0.42;
        if (r.level == -1) y = base + fm.ascent() * 0.22;
        qreal w = runWidth(r, f);
        if (r.box) {
            qreal h = rfm.ascent() * 0.72, bw = w - 2;
            QColor fill = ink, edge = ink;
            fill.setAlphaF(ink.alphaF() * 0.35);
            edge.setAlphaF(ink.alphaF() * 0.55);
            p.setRenderHint(QPainter::Antialiasing);
            p.setPen(QPen(edge, 0.8));
            p.setBrush(fill);
            p.drawRoundedRect(QRectF(x + 1.5, y - h + 0.5, bw - 1, h - 1), 1.5, 1.5);
        } else {
            p.setPen(ink);
            p.setFont(rf);
            p.drawText(QPointF(x, y), r.text);
        }
        x += w;
    }
    p.restore();
}

void drawRichFit(QPainter &p, const QRectF &rect, const QString &markup, QFont f, Qt::Alignment align, int minPx)
{
    if (f.pixelSize() > 0)
        while (f.pixelSize() > minPx && richWidth(markup, f) > rect.width()) f.setPixelSize(f.pixelSize() - 1);
    drawRich(p, rect, markup, f, align);
}

QString fontSafe(const QString &s, const QFont &f)
{
    QFontMetrics fm(f);
    QString out;
    for (QChar c : s) {
        if (c.unicode() < 0x80 || fm.inFontUcs4(c.unicode())) {
            out += c;
            continue;
        }
        if (superscripts().contains(c)) {
            QChar v = superscripts().value(c);
            out += '^';
            out += v == QChar(0x2212) ? QChar('-') : v;
        } else if (subscripts().contains(c)) {
            QChar v = subscripts().value(c);
            out += '_';
            out += v == QChar(0x2212) ? QChar('-') : v;
        } else if (c == QChar(0x2610)) {
            out += "[]";
        } else if (c == QChar(0x221B)) {
            out += "3√";
        } else if (c == QChar(0x2212)) {
            out += '-';
        } else if (c == QChar(0x231F)) {
            out += '_';
        } else {
            out += c;   // let Qt's font fallback try
        }
    }
    return out;
}

void makeFontSafe(QWidget *root)
{
    for (auto *c : root->findChildren<QComboBox *>())
        for (int i = 0; i < c->count(); ++i) c->setItemText(i, fontSafe(c->itemText(i), c->font()));
    for (auto *l : root->findChildren<QLabel *>())
        if (l->textFormat() != Qt::RichText && !Qt::mightBeRichText(l->text())) l->setText(fontSafe(l->text(), l->font()));
}

void RichDelegate::paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &idx) const
{
    QStyleOptionViewItem o = opt;
    initStyleOption(&o, idx);
    const QString text = o.text;
    o.text.clear();
    const QWidget *w = o.widget;
    QStyle *style = w ? w->style() : QApplication::style();
    style->drawControl(QStyle::CE_ItemViewItem, &o, p, w);
    if (text.isEmpty()) return;
    QRect r = style->subElementRect(QStyle::SE_ItemViewItemText, &o, w).adjusted(3, 0, -3, 0);
    QColor ink = (o.state & QStyle::State_Selected) ? o.palette.highlightedText().color() : o.palette.text().color();
    QVariant fg = idx.data(Qt::ForegroundRole);
    if (fg.isValid() && !(o.state & QStyle::State_Selected)) ink = fg.value<QBrush>().color();
    p->save();
    p->setPen(ink);
    p->setClipRect(r);
    drawRich(*p, r, unicodeToMarkup(text), o.font, o.displayAlignment);
    p->restore();
}
