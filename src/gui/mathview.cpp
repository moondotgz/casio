#include "mathview.h"
#include "core/constants.h"
#include "core/error.h"
#include "core/parser.h"
#include "richtext.h"
#include "theme.h"
#include <QFontDatabase>
#include <QFontMetricsF>
#include <QMouseEvent>
#include <QPainterPath>
#include <cmath>

MathStyle::MathStyle()
{
    static const QString fam = [] {
        const QStringList prefs = {"DejaVu Sans", "Noto Sans", "Liberation Sans", "Arial"};
        const QStringList have = QFontDatabase::families();
        for (const auto &p : prefs)
            if (have.contains(p)) return p;
        return QFontDatabase::systemFont(QFontDatabase::GeneralFont).family();
    }();
    family = appearance().mathFont.isEmpty() ? fam : appearance().mathFont;
}

namespace {

QFont makeFont(const MathStyle &st, qreal px, bool italic, bool bold)
{
    QFont f(st.family);
    f.setPixelSize(std::max(6, int(std::round(px))));
    f.setItalic(italic);
    f.setBold(bold);
    return f;
}

// ---------------------------------------------------------------- box types

class TextBox : public Box {
public:
    QString t;
    QFont f;
    QColor c;
    TextBox(const QString &text, const QFont &font, const QColor &col) : t(text), f(font), c(col)
    {
        QFontMetricsF fm(f);
        w = fm.horizontalAdvance(t);
        asc = fm.ascent() * 0.86;
        desc = fm.descent() * 0.8;
    }
    void draw(QPainter &p, qreal x, qreal y) const override
    {
        p.setFont(f);   // drawn with the painter's current ink
        p.drawText(QPointF(x, y), t);
    }
};

// Text with ^{} / _{} markup drawn without needing superscript glyphs in the font.
class RichBox : public Box {
public:
    QString m;
    QFont f;
    RichBox(const QString &markup, const QFont &font) : m(markup), f(font)
    {
        QFontMetricsF fm(f);
        w = richWidth(m, f);
        asc = fm.ascent() * 0.86;
        desc = fm.descent() * 0.8;
        if (m.contains("^{")) asc += fm.ascent() * 0.3;
        if (m.contains("_{")) desc += fm.ascent() * 0.2;
    }
    void draw(QPainter &p, qreal x, qreal y) const override
    {
        QFontMetricsF fm(f);
        // drawRich centres on the rect; give it a rect whose baseline is y
        drawRich(p, QRectF(x, y - fm.ascent(), w + 1, fm.ascent() + fm.descent()), m, f, Qt::AlignLeft | Qt::AlignTop);
    }
};

class SpaceBox : public Box {
public:
    explicit SpaceBox(qreal width) { w = width; }
    void draw(QPainter &, qreal, qreal) const override {}
};

class RowBox : public Box {
public:
    std::vector<BoxPtr> kids;
    qreal gap;
    explicit RowBox(std::vector<BoxPtr> k, qreal g = 0) : kids(std::move(k)), gap(g)
    {
        for (size_t i = 0; i < kids.size(); ++i) {
            const auto &b = kids[i];
            if (!b) continue;
            w += b->w + (i ? gap : 0);
            asc = std::max(asc, b->asc);
            desc = std::max(desc, b->desc);
        }
    }
    void draw(QPainter &p, qreal x, qreal y) const override
    {
        for (const auto &b : kids) {
            if (!b) continue;
            b->draw(p, x, y);
            x += b->w + gap;
        }
    }
};

class LinesBox : public Box {
public:
    std::vector<BoxPtr> lines;
    bool right;
    qreal gap;
    LinesBox(std::vector<BoxPtr> l, bool r, qreal g) : lines(std::move(l)), right(r), gap(g)
    {
        qreal h = 0;
        for (size_t i = 0; i < lines.size(); ++i) {
            w = std::max(w, lines[i]->w);
            h += lines[i]->height() + (i ? gap : 0);
        }
        // baseline of the box = baseline of the first line
        asc = lines.empty() ? 0 : lines[0]->asc;
        desc = h - asc;
    }
    void draw(QPainter &p, qreal x, qreal y) const override
    {
        qreal top = y - asc;
        for (const auto &l : lines) {
            qreal bx = right ? x + w - l->w : x;
            l->draw(p, bx, top + l->asc);
            top += l->height() + gap;
        }
    }
};

class FracBox : public Box {
public:
    BoxPtr num, den;
    QColor c;
    qreal axis, thick, gapv, pad;
    FracBox(BoxPtr n, BoxPtr d, qreal size, QColor col) : num(std::move(n)), den(std::move(d)), c(col)
    {
        axis = size * 0.30;
        thick = std::max<qreal>(1.0, size / 16.0);
        gapv = size * 0.10;
        pad = size * 0.12;
        w = std::max(num->w, den->w) + 2 * pad;
        asc = axis + thick / 2 + gapv + num->height();
        desc = std::max<qreal>(0, -axis + thick / 2 + gapv + den->height());
    }
    void draw(QPainter &p, qreal x, qreal y) const override
    {
        qreal ly = y - axis;
        p.fillRect(QRectF(x + pad * 0.4, ly - thick / 2, w - pad * 0.8, thick), p.pen().color());
        num->draw(p, x + (w - num->w) / 2, ly - thick / 2 - gapv - num->desc);
        den->draw(p, x + (w - den->w) / 2, ly + thick / 2 + gapv + den->asc);
    }
};

class SqrtBox : public Box {
public:
    BoxPtr body, index;
    QColor c;
    qreal rw, gapv, thick, ix = 0;
    SqrtBox(BoxPtr b, BoxPtr idx, qreal size, QColor col) : body(std::move(b)), index(std::move(idx)), c(col)
    {
        rw = size * 0.55;
        gapv = size * 0.12;
        thick = std::max<qreal>(1.0, size / 16.0);
        asc = body->asc + gapv + thick;
        desc = body->desc;
        if (index) ix = std::max<qreal>(0, index->w - rw * 0.45);
        w = ix + rw + body->w + size * 0.08;
        if (index) asc = std::max(asc, (asc + desc) * 0.45 + index->height() - desc + 2);
    }
    void draw(QPainter &p, qreal x, qreal y) const override
    {
        qreal top = y - body->asc - gapv - thick / 2;
        qreal bot = y + desc;
        qreal h = bot - top;
        qreal x0 = x + ix;
        QPainterPath path;
        path.moveTo(x0, bot - h * 0.42);
        path.lineTo(x0 + rw * 0.25, bot - h * 0.50);
        path.lineTo(x0 + rw * 0.55, bot);
        path.lineTo(x0 + rw * 0.95, top);
        path.lineTo(x + w, top);
        QPen ink = p.pen();
        p.setPen(QPen(ink.color(), thick, Qt::SolidLine, Qt::FlatCap, Qt::MiterJoin));
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
        p.setPen(ink);
        if (index) index->draw(p, x, bot - h * 0.50 - index->desc - 2);
        body->draw(p, x0 + rw, y);
    }
};

class ScriptBox : public Box {   // superscript and/or subscript
public:
    BoxPtr base, sup, sub;
    qreal supShift = 0, subShift = 0, sp;
    ScriptBox(BoxPtr b, BoxPtr up, BoxPtr down, qreal size) : base(std::move(b)), sup(std::move(up)), sub(std::move(down))
    {
        sp = size * 0.04;
        asc = base->asc;
        desc = base->desc;
        qreal sw = 0;
        if (sup) {
            supShift = std::max(base->asc * 0.55, size * 0.42) ;
            supShift = std::max(supShift, base->asc - sup->asc * 0.5);
            asc = std::max(asc, supShift + sup->asc);
            sw = std::max(sw, sup->w);
        }
        if (sub) {
            subShift = size * 0.28;
            desc = std::max(desc, subShift + sub->desc);
            sw = std::max(sw, sub->w);
        }
        w = base->w + sp + sw;
    }
    void draw(QPainter &p, qreal x, qreal y) const override
    {
        base->draw(p, x, y);
        if (sup) sup->draw(p, x + base->w + sp, y - supShift);
        if (sub) sub->draw(p, x + base->w + sp, y + subShift);
    }
};

class FenceBox : public Box {   // ( ), | |, [ ]
public:
    BoxPtr body;
    QChar open, close;
    QColor c;
    qreal fw, thick;
    FenceBox(BoxPtr b, QChar o, QChar cl, qreal size, QColor col) : body(std::move(b)), open(o), close(cl), c(col)
    {
        fw = (o == '|') ? size * 0.22 : size * 0.32;
        thick = std::max<qreal>(1.0, size / 15.0);
        asc = std::max(body->asc, size * 0.72) + size * 0.06;
        desc = std::max(body->desc, size * 0.2) + size * 0.06;
        w = body->w + (open.isNull() ? 0 : fw) + (close.isNull() ? 0 : fw);
    }
    void drawFence(QPainter &p, QChar ch, qreal x, qreal top, qreal bot, bool left) const
    {
        QPainterPath path;
        qreal h = bot - top;
        if (ch == '(') {
            qreal xi = left ? x + fw * 0.8 : x + fw * 0.2;
            qreal xo = left ? x + fw * 0.15 : x + fw * 0.85;
            path.moveTo(xi, top);
            path.cubicTo(xo, top + h * 0.2, xo, bot - h * 0.2, xi, bot);
        } else if (ch == '[') {
            qreal xi = left ? x + fw * 0.75 : x + fw * 0.25;
            qreal xo = left ? x + fw * 0.25 : x + fw * 0.75;
            path.moveTo(xi, top);
            path.lineTo(xo, top);
            path.lineTo(xo, bot);
            path.lineTo(xi, bot);
        } else if (ch == '|') {
            path.moveTo(x + fw / 2, top);
            path.lineTo(x + fw / 2, bot);
        }
        QPen ink = p.pen();
        p.setPen(QPen(ink.color(), thick, Qt::SolidLine, Qt::RoundCap, Qt::RoundJoin));
        p.setBrush(Qt::NoBrush);
        p.drawPath(path);
        p.setPen(ink);
    }
    void draw(QPainter &p, qreal x, qreal y) const override
    {
        qreal top = y - asc + thick, bot = y + desc - thick;
        qreal bx = x;
        if (!open.isNull()) {
            drawFence(p, open == ')' ? '(' : open, x, top, bot, true);
            bx += fw;
        }
        body->draw(p, bx, y);
        if (!close.isNull()) drawFence(p, close == ')' ? '(' : (close == ']' ? '[' : close), bx + body->w, top, bot, false);
    }
};

class BigOpBox : public Box {   // ∫ Σ Π with limits
public:
    BoxPtr sym, upper, lower;
    bool sideLimits;   // ∫: limits to the right; Σ/Π: above/below
    qreal symW;
    BigOpBox(BoxPtr s, BoxPtr up, BoxPtr lo, bool side, qreal size) : sym(std::move(s)), upper(std::move(up)), lower(std::move(lo)), sideLimits(side)
    {
        symW = sym->w;
        if (sideLimits) {
            w = symW + std::max(upper ? upper->w : 0, lower ? lower->w : 0) + size * 0.1;
            asc = std::max(sym->asc, size * 0.5 + (upper ? upper->height() : 0));
            desc = std::max(sym->desc, (lower ? lower->height() : 0) + size * 0.05);
        } else {
            w = std::max({symW, upper ? upper->w : 0.0, lower ? lower->w : 0.0});
            asc = sym->asc + (upper ? upper->height() + 1 : 0);
            desc = sym->desc + (lower ? lower->height() + 1 : 0);
        }
    }
    void draw(QPainter &p, qreal x, qreal y) const override
    {
        if (sideLimits) {
            sym->draw(p, x, y);
            if (upper) upper->draw(p, x + symW * 0.85, y - asc + upper->asc);
            if (lower) lower->draw(p, x + symW * 0.55, y + desc - lower->desc);
        } else {
            sym->draw(p, x + (w - symW) / 2, y);
            if (upper) upper->draw(p, x + (w - upper->w) / 2, y - sym->asc - 1 - upper->desc);
            if (lower) lower->draw(p, x + (w - lower->w) / 2, y + sym->desc + 1 + lower->asc);
        }
    }
};

class EvalBarBox : public Box {   // body |_(x=a)  (derivative evaluation point)
public:
    BoxPtr body, sub;
    QColor c;
    qreal thick, sp;
    EvalBarBox(BoxPtr b, BoxPtr s, qreal size, QColor col) : body(std::move(b)), sub(std::move(s)), c(col)
    {
        thick = std::max<qreal>(1.0, size / 15.0);
        sp = size * 0.12;
        asc = body->asc;
        desc = std::max(body->desc, size * 0.25 + sub->height() * 0.6);
        w = body->w + sp * 2 + sub->w;
    }
    void draw(QPainter &p, qreal x, qreal y) const override
    {
        body->draw(p, x, y);
        qreal bx = x + body->w + sp * 0.6;
        QPen ink = p.pen();
        p.setPen(QPen(ink.color(), thick));
        p.drawLine(QPointF(bx, y - asc), QPointF(bx, y + desc));
        p.setPen(ink);
        sub->draw(p, bx + sp, y + desc - sub->desc);
    }
};

class GridBox : public Box {   // matrix / vector body (without brackets)
public:
    std::vector<std::vector<BoxPtr>> cells;
    std::vector<qreal> colW, rowAsc, rowDesc;
    qreal hgap, vgap, axis;
    GridBox(std::vector<std::vector<BoxPtr>> c, qreal size) : cells(std::move(c))
    {
        hgap = size * 0.7;
        vgap = size * 0.25;
        axis = size * 0.3;
        size_t R = cells.size(), C = R ? cells[0].size() : 0;
        colW.assign(C, 0);
        rowAsc.assign(R, 0);
        rowDesc.assign(R, 0);
        for (size_t r = 0; r < R; ++r)
            for (size_t k = 0; k < C; ++k) {
                colW[k] = std::max(colW[k], cells[r][k]->w);
                rowAsc[r] = std::max(rowAsc[r], cells[r][k]->asc);
                rowDesc[r] = std::max(rowDesc[r], cells[r][k]->desc);
            }
        for (size_t k = 0; k < C; ++k) w += colW[k] + (k ? hgap : 0);
        qreal h = 0;
        for (size_t r = 0; r < R; ++r) h += rowAsc[r] + rowDesc[r] + (r ? vgap : 0);
        w += size * 0.2;
        asc = h / 2 + axis;
        desc = h / 2 - axis;
    }
    void draw(QPainter &p, qreal x, qreal y) const override
    {
        qreal top = y - asc;
        for (size_t r = 0; r < cells.size(); ++r) {
            qreal base = top + rowAsc[r];
            qreal cx = x + w * 0 + 0.1 * hgap;
            for (size_t k = 0; k < cells[r].size(); ++k) {
                const auto &b = cells[r][k];
                b->draw(p, cx + (colW[k] - b->w) / 2, base);
                cx += colW[k] + hgap;
            }
            top += rowAsc[r] + rowDesc[r] + vgap;
        }
    }
};

// ---------------------------------------------------------------- AST → boxes

class Builder {
public:
    const MathStyle &st;
    explicit Builder(const MathStyle &s) : st(s) {}

    qreal sz(int level) const { return st.size * std::max(0.55, std::pow(0.72, level)); }

    BoxPtr text(const QString &t, int level, bool italic = false, bool bold = false) const
    {
        return std::make_shared<TextBox>(t, makeFont(st, sz(level), italic, bold), st.color);
    }
    BoxPtr space(int level, qreal em) const { return std::make_shared<SpaceBox>(sz(level) * em); }
    BoxPtr row(std::vector<BoxPtr> v) const { return std::make_shared<RowBox>(std::move(v)); }
    BoxPtr op(const QString &s, int level, qreal em = 0.18) const
    {
        return row({space(level, em), text(s, level), space(level, em)});
    }
    BoxPtr paren(BoxPtr b, int level, bool closed = true) const
    {
        return std::make_shared<FenceBox>(std::move(b), '(', closed ? ')' : QChar(), sz(level), st.color);
    }

    QString numberText(const QString &raw) const
    {
        QString s = raw;
        s.replace('-', QChar(0x2212));
        if (!st.settings) return s;
        QChar dec = st.settings->decimalComma ? ',' : '.';
        QChar sep = st.settings->decimalComma ? '.' : ',';
        int dot = int(s.indexOf('.'));
        QString ip = dot >= 0 ? s.left(dot) : s, fp = dot >= 0 ? s.mid(dot + 1) : QString();
        if (st.settings->digitSeparator && !s.contains(QChar(0x00B0))) {
            QString sign;
            if (ip.startsWith(QChar(0x2212))) { sign = ip.left(1); ip = ip.mid(1); }
            QString g;
            for (int i = 0; i < ip.size(); ++i) {
                if (i && (ip.size() - i) % 3 == 0) g += sep;
                g += ip[i];
            }
            ip = sign + g;
        }
        return dot >= 0 ? ip + dec + fp : ip;
    }

    BoxPtr number(const NodePtr &n, int level) const
    {
        QString t = n->text.isEmpty() ? QString::number(n->num) : n->text;
        int e = int(t.indexOf(QChar(0x1D07)));
        if (e >= 0 && !t.contains(QChar(0x00B0))) {
            BoxPtr mant = text(numberText(t.left(e)) + QChar(0x00D7) + "10", level);
            BoxPtr ex = text(numberText(t.mid(e + 1)), level + 1);
            return std::make_shared<ScriptBox>(mant, ex, nullptr, sz(level));
        }
        return text(numberText(t), level);
    }

    BoxPtr strip(const NodePtr &n, int level) const
    {
        if ((n->type == Node::Type::Paren && n->closed) || n->type == Node::Type::Group) return build(n->kids[0], level);
        return build(n, level);
    }

    BoxPtr args(const NodePtr &n, int level, size_t from = 0) const
    {
        std::vector<BoxPtr> v;
        for (size_t i = from; i < n->kids.size(); ++i) {
            if (i > from) v.push_back(text(",", level));
            v.push_back(build(n->kids[i], level));
        }
        if (v.empty()) v.push_back(space(level, 0.2));
        return row(v);
    }

    static QString varDisplay(const QString &v)
    {
        if (v == "i") return QStringLiteral("i");
        return v;
    }

    BoxPtr build(const NodePtr &n, int level) const
    {
        switch (n->type) {
        case Node::Type::Num: return number(n, level);
        case Node::Type::Var: {
            bool it = n->op == "x" || n->op == "y" || n->op == "i" || n->op == "e";
            return text(varDisplay(n->op), level, it);
        }
        case Node::Type::Const: {
            const SciConstant *c = findConstant(n->op);
            return std::make_shared<RichBox>(unicodeToMarkup(c ? c->symbol : n->op), makeFont(st, sz(level), true, false));
        }
        case Node::Type::Cell:
        case Node::Type::Range: return text(n->op, level);
        case Node::Type::Unary: return row({text(QStringLiteral("−"), level), build(n->kids[0], level)});
        case Node::Type::Paren: return paren(build(n->kids[0], level), level, n->closed);
        case Node::Type::Group: return build(n->kids[0], level);
        case Node::Type::Assign:
            return row({build(n->kids[0], level), op(QStringLiteral("→"), level, 0.1), text(n->op, level)});
        case Node::Type::Binary: return binary(n, level);
        case Node::Type::Postfix: return postfix(n, level);
        case Node::Type::Call: return call(n, level);
        }
        return text("?", level);
    }

    BoxPtr binary(const NodePtr &n, int level) const
    {
        const QString &o = n->op;
        BoxPtr l = build(n->kids[0], level);
        if (o == "/") {
            return std::make_shared<FracBox>(strip(n->kids[0], level + (level ? 1 : 0)),
                                             strip(n->kids[1], level + (level ? 1 : 0)), sz(level), st.color);
        }
        if (o == "^") {
            BoxPtr ex = strip(n->kids[1], level + 1);
            return std::make_shared<ScriptBox>(l, ex, nullptr, sz(level));
        }
        BoxPtr r = build(n->kids[1], level);
        if (o == "·") {
            // keep a visible × between two plain numbers so 2·3 doesn't read as 23
            if (n->kids[0]->type == Node::Type::Num && n->kids[1]->type == Node::Type::Num)
                return row({l, op(QStringLiteral("×"), level), r});
            return row({l, r});
        }
        if (o == "nPr" || o == "nCr") return row({l, text(o.mid(1, 1), level, false, true), r});
        if (o == "and" || o == "or" || o == "xor" || o == "xnor") return row({l, op(o, level, 0.3), r});
        if (o == "∠") return row({l, text(o, level), r});
        return row({l, op(o, level), r});
    }

    BoxPtr postfix(const NodePtr &n, int level) const
    {
        const QString &o = n->op;
        BoxPtr b = build(n->kids[0], level);
        if (o == QStringLiteral("⁻¹"))
            return std::make_shared<ScriptBox>(b, text(QStringLiteral("−") + "1", level + 1), nullptr, sz(level));
        if (o == QStringLiteral("²")) return std::make_shared<ScriptBox>(b, text("2", level + 1), nullptr, sz(level));
        if (o == QStringLiteral("³")) return std::make_shared<ScriptBox>(b, text("3", level + 1), nullptr, sz(level));
        if (o == "conv") return row({b, space(level, 0.1), text(n->text, level + 1)});
        if (o.startsWith(QChar(0x25B6))) return row({b, text(o, level)});
        return row({b, text(o, level)});
    }

    BoxPtr call(const NodePtr &n, int level) const
    {
        const QString &f = n->op;
        const auto &k = n->kids;
        qreal s = sz(level);
        if (f == QStringLiteral("√") && k.size() == 1)
            return std::make_shared<SqrtBox>(strip(k[0], level), nullptr, s, st.color);
        if (f == QStringLiteral("∛") && k.size() == 1)
            return std::make_shared<SqrtBox>(strip(k[0], level), text("3", level + 2), s, st.color);
        if (f == "root" && k.size() == 2)
            return std::make_shared<SqrtBox>(strip(k[1], level), build(k[0], level + 2), s, st.color);
        if (f == "Abs" && k.size() == 1)
            return std::make_shared<FenceBox>(strip(k[0], level), '|', '|', s, st.color);
        if (f == "log" && k.size() == 2) {
            BoxPtr lg = std::make_shared<ScriptBox>(text("log", level), nullptr, build(k[0], level + 1), s);
            return row({lg, paren(strip(k[1], level), level, n->closed)});
        }
        if (f == "exp" && k.size() == 1)
            return std::make_shared<ScriptBox>(text("e", level, true), strip(k[0], level + 1), nullptr, s);
        if (f == "mixed" && k.size() == 3) {
            BoxPtr fr = std::make_shared<FracBox>(build(k[1], level + 1), build(k[2], level + 1), sz(level + 1), st.color);
            return row({build(k[0], level), fr});
        }
        if (f == QStringLiteral("∫") && k.size() == 3) {
            BoxPtr sym = text(QStringLiteral("∫"), level, false);
            auto big = std::make_shared<TextBox>(QStringLiteral("∫"), makeFont(st, s * 1.5, false, false), st.color);
            BoxPtr op = std::make_shared<BigOpBox>(big, build(k[2], level + 1), build(k[1], level + 1), true, s);
            return row({op, space(level, 0.1), build(k[0], level), space(level, 0.12), text("d", level), text("x", level, true)});
        }
        if ((f == QStringLiteral("Σ") || f == QStringLiteral("Π")) && k.size() == 3) {
            auto big = std::make_shared<TextBox>(f, makeFont(st, s * 1.3, false, false), st.color);
            BoxPtr lower = row({text("x", level + 1, true), text("=", level + 1), build(k[1], level + 1)});
            BoxPtr op = std::make_shared<BigOpBox>(big, build(k[2], level + 1), lower, false, s);
            return row({op, space(level, 0.08), paren(build(k[0], level), level)});
        }
        if (f == "d/dx" && k.size() == 2) {
            BoxPtr dd = std::make_shared<FracBox>(text("d", level), row({text("d", level), text("x", level, true)}), s, st.color);
            BoxPtr sub = row({text("x", level + 1, true), text("=", level + 1), build(k[1], level + 1)});
            return row({dd, std::make_shared<EvalBarBox>(paren(build(k[0], level), level), sub, s, st.color)});
        }
        if (f == "Ran#") return text("Ran#", level);
        if (n->kids.size() == 1 && !n->closed && n->kids[0]->type != Node::Type::Paren) {
            // function typed without parenthesis (e.g. sin30)
            return row({text(f, level), space(level, 0.08), build(k[0], level)});
        }
        return row({text(f, level), paren(args(n, level), level, n->closed)});
    }
};

} // namespace

// ---------------------------------------------------------------- public layout API

BoxPtr layoutText(const QString &t, const MathStyle &st, qreal scale, bool italic, bool bold)
{
    return std::make_shared<TextBox>(t, makeFont(st, st.size * scale, italic, bold), st.color);
}

BoxPtr layoutRow(std::vector<BoxPtr> items, qreal gap) { return std::make_shared<RowBox>(std::move(items), gap); }

BoxPtr layoutLines(std::vector<BoxPtr> lines, bool rightAlign, qreal gap)
{
    return std::make_shared<LinesBox>(std::move(lines), rightAlign, gap);
}

BoxPtr layoutSyntax(const QString &syntax, const MathStyle &st, const LexOptions &lex)
{
    if (syntax.isEmpty()) return nullptr;
    if (!st.natural)
        return std::make_shared<RichBox>(unicodeToMarkup(toDisplayText(syntax)), makeFont(st, st.size, false, false));
    try {
        ParseOptions po;
        po.lex = lex;
        po.allowRelations = true;
        NodePtr n = parseExpression(syntax, po);
        Builder b(st);
        return b.build(n, 0);
    } catch (const CalcError &) {
        // multi-statement or incomplete input: render each ':'-separated part, falling back to text
        if (syntax.contains(':')) {
            try {
                ParseOptions po;
                po.lex = lex;
                auto stmts = parseStatements(syntax, po);
                Builder b(st);
                std::vector<BoxPtr> parts;
                for (size_t i = 0; i < stmts.size(); ++i) {
                    if (i) parts.push_back(b.text(":", 0));
                    parts.push_back(b.build(stmts[i], 0));
                }
                return layoutRow(parts);
            } catch (const CalcError &) {
            }
        }
        return layoutText(toLinearText(syntax), st);
    }
}

BoxPtr layoutLabeled(const QString &labelSyntax, const BoxPtr &value, const MathStyle &st)
{
    Builder b(st);
    bool letter = labelSyntax.size() == 1 && labelSyntax[0].isLower();
    BoxPtr lab = letter ? layoutText(labelSyntax, st, 1.0, true) : layoutSyntax(labelSyntax, st);
    return layoutRow({lab, b.op("=", 0, 0.15), value});
}

BoxPtr layoutValue(const Value &v, const Settings &s, const DisplayOpts &o, const MathStyle &st)
{
    if (v.isScalar()) return layoutSyntax(formatComplex(v.z, s, o), st);
    MathStyle cs = st;
    cs.size = st.size * (v.rows > 2 ? 0.8 : 0.9);
    std::vector<std::vector<BoxPtr>> cells;
    int R = v.rows, C = v.cols;
    for (int r = 0; r < R; ++r) {
        std::vector<BoxPtr> rowv;
        for (int c = 0; c < C; ++c) {
            BoxPtr b = layoutSyntax(formatComplex(v.at(r, c), s, o), cs);
            rowv.push_back(b ? b : layoutText("0", cs));
        }
        cells.push_back(rowv);
    }
    auto grid = std::make_shared<GridBox>(cells, cs.size);
    return std::make_shared<FenceBox>(grid, '[', ']', cs.size, st.color);
}

// ---------------------------------------------------------------- widget

MathView::MathView(QWidget *parent) : QWidget(parent)
{
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Preferred);
}

void MathView::setBox(BoxPtr b)
{
    m_box = std::move(b);
    updateGeometry();
    update();
}

QSize MathView::sizeHint() const
{
    qreal lineH = m_style.size * appearance().mathScale * 1.35;
    qreal h = std::max(lineH * m_minLines, m_box ? m_box->height() + 8 : lineH);
    qreal w = m_box ? m_box->w + 12 : 40;
    return QSize(int(std::ceil(w)), int(std::ceil(h)));
}

QSize MathView::minimumSizeHint() const
{
    QSize s = sizeHint();
    return QSize(20, s.height());
}

void MathView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.setRenderHint(QPainter::TextAntialiasing);
    p.setPen(themeColor("lcdText"));
    if (m_inverted) {
        p.fillRect(rect(), themeColor("lcdText"));
        p.setPen(themeColor("lcdBg"));
    }
    if (!m_box) return;
    const qreal margin = 6;
    qreal avail = width() - 2 * margin;
    qreal scale = 1.0;
    if (m_box->w > avail && m_box->w > 0) scale = std::max(0.4, avail / m_box->w);
    qreal bw = m_box->w * scale, bh = m_box->height() * scale;
    qreal x;
    if (m_align & Qt::AlignRight) x = width() - margin - bw;
    else if (m_align & Qt::AlignHCenter) x = (width() - bw) / 2;
    else x = margin;
    if (bw > avail) x = (m_align & Qt::AlignRight) ? width() - margin - bw : margin;
    qreal top = (m_align & Qt::AlignTop) ? margin * 0.5 : (height() - bh) / 2;
    p.save();
    p.translate(x, top + m_box->asc * scale);
    p.scale(scale, scale);
    m_box->draw(p, 0, 0);
    p.restore();
}

void MathView::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) emit clicked();
    QWidget::mousePressEvent(e);
}
