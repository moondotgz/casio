#include "keypad.h"
#include "richtext.h"
#include "theme.h"
#include <QGridLayout>
#include <QHash>
#include <QLinearGradient>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QtMath>

namespace {


QVector<KeyDef> buildKeys()
{
    using S = KeyStyle;
    auto K = [](QString id, QString l, QString a, QString sl = {}, QString sa = {}, QString al = {}, QString aa = {},
                S st = S::Function) {
        KeyDef d;
        d.id = std::move(id);
        d.label = std::move(l);
        d.action = std::move(a);
        d.shiftLabel = std::move(sl);
        d.shiftAction = std::move(sa);
        d.alphaLabel = std::move(al);
        d.alphaAction = std::move(aa);
        d.style = st;
        return d;
    };
    QVector<KeyDef> v;
    // --- top block (6 columns, nav pad in the middle of the first two rows)
    v << K("shift", "SHIFT", "c:SHIFT", {}, {}, {}, {}, S::Modifier)
      << K("alpha", "ALPHA", "c:ALPHA", {}, {}, {}, {}, S::Modifier)
      << K("menu", "MENU", "c:MENU", "SETUP", "c:SETUP", "THEME", "c:CUSTOMIZE")
      << K("on", "ON", "c:AC", "OFF", "c:OFF");
    v << K("optn", "OPTN", "c:OPTN", "CATALOG", "c:CATALOG")
      << K("calc", "CALC", "c:CALC", "SOLVE", "c:SOLVE", "=", "i:=")
      << K("integ", "\u222B\u2610", "i:\u222B(\u00A7,,)", "d/dx", "i:d/dx(\u00A7,)", ":", "i::")
      << K("xvar", "x", "i:x", "\u03A3", "i:\u03A3(\u00A7,,)", "\u03A0", "i:\u03A0(\u00A7,,)");
    v << K("frac", "\u2610/\u2610", "i:/", "[]^{[]}/_{[]}", "i:\u231F")
      << K("sqrt", "\u221A\u2610", "i:\u221A(\u00A7)", "\u221B\u2610", "i:\u221B(\u00A7)")
      << K("sq", "x\u00B2", "i:\u00B2", "x\u00B3", "i:\u00B3")
      << K("pow", "x\u2610", "i:^(\u00A7)", "\u207F\u221A\u2610", "i:root(\u00A7,)")
      << K("log", "log", "i:log(\u00A7)", "10\u2610", "i:10^(\u00A7)", "log\u2090b", "i:log(\u00A7,)")
      << K("ln", "ln", "i:ln(\u00A7)", "e\u2610", "i:e^(\u00A7)");
    v << K("neg", "(\u2212)", "n:\u2212", "\u2220", "i:\u2220", "A", "i:A")
      << K("dms", "\u00B0'\"", "c:DMS", "FACT", "c:FACT", "B", "i:B")
      << K("inv", "x\u207B\u00B9", "i:\u207B\u00B9", "x!", "i:!", "C", "i:C")
      << K("sin", "sin", "i:sin(", "sin\u207B\u00B9", "i:sin\u207B\u00B9(", "D", "i:D")
      << K("cos", "cos", "i:cos(", "cos\u207B\u00B9", "i:cos\u207B\u00B9(", "E", "i:E")
      << K("tan", "tan", "i:tan(", "tan\u207B\u00B9", "i:tan\u207B\u00B9(", "F", "i:F");
    v << K("sto", "STO", "c:STO", "RCL", "c:RCL")
      << K("eng", "ENG", "c:ENG", "\u2190", "c:ENGL", "i", "i:i")
      << K("lp", "(", "n:(", "%", "i:%")
      << K("rp", ")", "i:)", ",", "i:,", "x", "i:x")
      << K("sd", "S\u21D4D", "c:SD", "a^{b}/_{c}\u21D4d/c", "c:FRACSWAP", "y", "i:y")
      << K("mplus", "M+", "c:M+", "M\u2212", "c:M-", "M", "i:M");
    // --- number block (5 columns)
    v << K("7", "7", "n:7", "CONST", "c:CONST", {}, {}, S::Number)
      << K("8", "8", "n:8", "CONV", "c:CONV", {}, {}, S::Number)
      << K("9", "9", "n:9", "RESET", "c:RESET", {}, {}, S::Number)
      << K("del", "DEL", "c:DEL", "UNDO", "c:UNDO", {}, {}, S::Accent)
      << K("ac", "AC", "c:AC", "OFF", "c:OFF", {}, {}, S::Accent);
    v << K("4", "4", "n:4", {}, {}, {}, {}, S::Number)
      << K("5", "5", "n:5", {}, {}, {}, {}, S::Number)
      << K("6", "6", "n:6", {}, {}, {}, {}, S::Number)
      << K("mul", "\u00D7", "i:\u00D7", "nPr", "i:nPr", "GCD", "n:GCD(\u00A7,)", S::Number)
      << K("div", "\u00F7", "i:\u00F7", "nCr", "i:nCr", "LCM", "n:LCM(\u00A7,)", S::Number);
    v << K("1", "1", "n:1", {}, {}, {}, {}, S::Number)
      << K("2", "2", "n:2", {}, {}, {}, {}, S::Number)
      << K("3", "3", "n:3", {}, {}, {}, {}, S::Number)
      << K("add", "+", "i:+", "Pol", "n:Pol(\u00A7,)", {}, {}, S::Number)
      << K("sub", "\u2212", "i:\u2212", "Rec", "n:Rec(\u00A7,)", {}, {}, S::Number);
    v << K("0", "0", "n:0", "Rnd", "n:Rnd(\u00A7)", {}, {}, S::Number)
      << K("dot", ".", "n:.", "Ran#", "n:Ran#", "RanInt", "n:RanInt#(\u00A7,)", S::Number)
      << K("exp", "\u00D710\u02E3", "i:\u1D07", "\u03C0", "n:\u03C0", "e", "n:e", S::Number)
      << K("ans", "Ans", "n:Ans", "PreAns", "n:PreAns", {}, {}, S::Number)
      << K("exe", "=", "c:EXE", "\u2248", "c:APPROX", {}, {}, S::Number);
    return v;
}

} // namespace

// Letter versions of the template legends (used when Appearance::letterTemplates is on).
QString keyText(const KeyDef &d, KeyLayer layer)
{
    const QString &raw = layer == KeyLayer::Shift ? d.shiftLabel : layer == KeyLayer::Alpha ? d.alphaLabel : d.label;
    if (appearance().letterTemplates) {
        static const QHash<QString, QString> letters = {
            {"frac|0", "a/b"},          {"frac|1", "a^{b}/_{c}"},
            {"integ|0", "\u222Bdx"},
            {"sqrt|0", "\u221Ax"},     {"sqrt|1", "^{3}\u221Ax"},
            {"pow|0", "x^{y}"},         {"pow|1", "^{x}\u221Ay"},
            {"log|1", "10^{x}"},        {"ln|1", "e^{x}"},
        };
        auto it = letters.find(d.id + "|" + QString::number(int(layer)));
        if (it != letters.end()) return *it;
    }
    return unicodeToMarkup(raw);
}

// ------------------------------------------------------------------ KeyButton

KeyButton::KeyButton(const KeyDef &def, QWidget *parent) : QWidget(parent), m_def(def)
{
    setFocusPolicy(Qt::NoFocus);
    setAttribute(Qt::WA_Hover);
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    auto plain = [this](QString t) {
        t.replace("^{[]}", "^\u2610").replace("[]", "\u2610").replace("^{", "^").replace("_{", "_").remove('}');
        return fontSafe(t, font());
    };
    QString tip = plain(keyText(def, KeyLayer::Main));
    if (!def.shiftLabel.isEmpty()) tip += "   SHIFT: " + plain(keyText(def, KeyLayer::Shift));
    if (!def.alphaLabel.isEmpty()) tip += "   ALPHA: " + plain(keyText(def, KeyLayer::Alpha));
    setToolTip(tip);
}

QSize KeyButton::sizeHint() const
{
    bool big = m_def.style == KeyStyle::Number || m_def.style == KeyStyle::Accent;
    int legend = appearance().showLegends ? 0 : 11;
    return big ? QSize(74, 52 - legend) : QSize(60, 44 - legend);
}

void KeyButton::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const Appearance &ap = appearance();
    const qreal ks = ap.keyScale;
    const qreal labelH = ap.showLegends ? std::round(13 * std::min(ks, 1.25)) : 2;
    QRectF r = QRectF(rect()).adjusted(3, labelH + 1, -3, -3);
    bool shiftOn = m_layer == KeyLayer::Shift, alphaOn = m_layer == KeyLayer::Alpha;

    // legends above the key
    if (ap.showLegends) {
        QFont lf = font();
        lf.setPixelSize(std::max(7, int(std::round(10 * std::min(ks, 1.25)))));
        lf.setBold(true);
        p.setFont(lf);
        if (!m_def.shiftLabel.isEmpty()) {
            QColor c = themeColor("legendShift");
            if (alphaOn) c.setAlpha(90);
            p.setPen(c);
            QRectF lr(2, 0, width() * (m_def.alphaLabel.isEmpty() ? 1.0 : 0.62) - 2, labelH);
            drawRichFit(p, lr, keyText(m_def, KeyLayer::Shift), lf, Qt::AlignLeft | Qt::AlignVCenter);
        }
        if (!m_def.alphaLabel.isEmpty()) {
            QColor c = themeColor("legendAlpha");
            if (shiftOn) c.setAlpha(90);
            p.setPen(c);
            drawRichFit(p, QRectF(width() * 0.5, 0, width() * 0.5 - 3, labelH), keyText(m_def, KeyLayer::Alpha), lf,
                        Qt::AlignRight | Qt::AlignVCenter);
        }
    }

    // key body
    QString role;
    switch (m_def.style) {
    case KeyStyle::Number: role = "keyNum"; break;
    case KeyStyle::Accent: role = "keyAccent"; break;
    case KeyStyle::Modifier: role = m_def.id == "shift" ? "keyShift" : "keyAlpha"; break;
    default: role = "keyFunc"; break;
    }
    const QColor base = themeColor(role), text = themeColor(role + "Text");
    QColor top = ap.keyGradient ? base.lighter(110) : base, bottom = ap.keyGradient ? base.darker(112) : base;
    if (m_hover) { top = top.lighter(108); bottom = bottom.lighter(108); }
    if (m_down || m_latched) { std::swap(top, bottom); top = top.darker(110); bottom = bottom.darker(104); }
    QLinearGradient g(r.topLeft(), r.bottomLeft());
    g.setColorAt(0, top);
    g.setColorAt(1, bottom);
    const qreal rad = std::min<qreal>(ap.keyRadius, r.height() / 2);
    QPainterPath path;
    path.addRoundedRect(r, rad, rad);
    p.fillPath(path, g);
    QColor border = text;
    border.setAlpha(base.lightness() < 60 ? 90 : 45);   // keeps dark keys visible on dark bodies
    p.setPen(QPen(border, 1));
    p.drawPath(path);
    if (m_latched) {
        QColor ring = text;
        ring.setAlpha(210);
        p.setPen(QPen(ring, 2));
        p.drawRoundedRect(r.adjusted(1.5, 1.5, -1.5, -1.5), std::max<qreal>(0, rad - 1), std::max<qreal>(0, rad - 1));
    }

    QFont mf = font();
    bool big = m_def.style == KeyStyle::Number;
    int px = big ? (m_def.label.size() > 2 ? 16 : 21) : (m_def.label.size() > 4 ? 12 : 15);
    mf.setPixelSize(std::max(7, int(std::round(px * ks))));
    mf.setBold(m_def.style == KeyStyle::Modifier || m_def.style == KeyStyle::Accent);
    p.setFont(mf);
    QColor tc = text;
    if ((shiftOn && !m_def.shiftLabel.isEmpty()) || (alphaOn && !m_def.alphaLabel.isEmpty())) tc.setAlpha(140);
    p.setPen(tc);
    drawRichFit(p, r.adjusted(3, 0, -3, 0).translated(0, m_down ? 1 : 0), keyText(m_def, KeyLayer::Main), mf, Qt::AlignCenter);
}

void KeyButton::mousePressEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton) {
        m_down = true;
        update();
    }
}

void KeyButton::mouseReleaseEvent(QMouseEvent *e)
{
    if (e->button() == Qt::LeftButton && m_down) {
        m_down = false;
        update();
        if (rect().contains(e->position().toPoint())) emit pressed(m_def);
    }
}

void KeyButton::enterEvent(QEnterEvent *) { m_hover = true; update(); }
void KeyButton::leaveEvent(QEvent *) { m_hover = false; m_down = false; update(); }

// ------------------------------------------------------------------ NavPad

NavPad::NavPad(QWidget *parent) : QWidget(parent)
{
    setFocusPolicy(Qt::NoFocus);
    setCursor(Qt::PointingHandCursor);
    setSizePolicy(QSizePolicy::Expanding, QSizePolicy::Expanding);
    setToolTip("Cursor / replay  (\u25C0 \u25B6 move cursor, \u25B2 \u25BC history)");
}

QString NavPad::hit(const QPointF &pt) const
{
    QPointF c(width() / 2.0, height() / 2.0);
    QPointF d = pt - c;
    // normalise for the elliptic pad shape
    double nx = d.x() / (width() / 2.0), ny = d.y() / (height() / 2.0);
    if (std::abs(nx) > std::abs(ny)) return nx < 0 ? "LEFT" : "RIGHT";
    return ny < 0 ? "UP" : "DOWN";
}

void NavPad::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    QRectF r = QRectF(rect()).adjusted(8, 6, -8, -6);
    const QColor pad = themeColor("navPad"), arrow = themeColor("navArrow");
    QRadialGradient g(r.center(), r.width() / 1.6);
    g.setColorAt(0, appearance().keyGradient ? pad.lighter(112) : pad);
    g.setColorAt(1, appearance().keyGradient ? pad.darker(118) : pad);
    p.setBrush(g);
    QColor rim = arrow;
    rim.setAlpha(110);
    p.setPen(QPen(rim, 1));
    p.drawEllipse(r);
    p.setBrush(themeColor("keyFunc"));
    p.drawEllipse(QRectF(r.center() - QPointF(r.width() * 0.16, r.height() * 0.16), QSizeF(r.width() * 0.32, r.height() * 0.32)));
    auto tri = [&](const QString &dir, QPointF c, qreal s) {
        QPolygonF poly;
        if (dir == "UP") poly << c + QPointF(0, -s) << c + QPointF(-s, s * 0.6) << c + QPointF(s, s * 0.6);
        if (dir == "DOWN") poly << c + QPointF(0, s) << c + QPointF(-s, -s * 0.6) << c + QPointF(s, -s * 0.6);
        if (dir == "LEFT") poly << c + QPointF(-s, 0) << c + QPointF(s * 0.6, -s) << c + QPointF(s * 0.6, s);
        if (dir == "RIGHT") poly << c + QPointF(s, 0) << c + QPointF(-s * 0.6, -s) << c + QPointF(-s * 0.6, s);
        p.setBrush(m_down == dir ? themeColor("keyAccent") : arrow);
        p.setPen(Qt::NoPen);
        p.drawPolygon(poly);
    };
    qreal s = std::min(r.width(), r.height()) * 0.09;
    QPointF c = r.center();
    tri("UP", c + QPointF(0, -r.height() * 0.33), s);
    tri("DOWN", c + QPointF(0, r.height() * 0.33), s);
    tri("LEFT", c + QPointF(-r.width() * 0.34, 0), s);
    tri("RIGHT", c + QPointF(r.width() * 0.34, 0), s);
}

void NavPad::mousePressEvent(QMouseEvent *e)
{
    m_down = hit(e->position());
    update();
}

void NavPad::mouseReleaseEvent(QMouseEvent *e)
{
    QString h = hit(e->position());
    if (h == m_down && rect().contains(e->position().toPoint())) emit nav(h);
    m_down.clear();
    update();
}

// ------------------------------------------------------------------ Keypad

Keypad::Keypad(QWidget *parent) : QWidget(parent)
{
    m_defs = buildKeys();
    auto *outer = new QVBoxLayout(this);
    outer->setContentsMargins(0, 0, 0, 0);
    outer->setSpacing(10);

    auto *top = new QGridLayout;
    top->setHorizontalSpacing(4);
    top->setVerticalSpacing(2);
    auto *bottom = new QGridLayout;
    bottom->setHorizontalSpacing(6);
    bottom->setVerticalSpacing(3);

    auto *pad = new NavPad(this);
    connect(pad, &NavPad::nav, this, &Keypad::nav);
    top->addWidget(pad, 0, 2, 2, 2);

    int topIndex = 0, bottomIndex = 0;
    for (const KeyDef &d : m_defs) {
        auto *b = new KeyButton(d, this);
        connect(b, &KeyButton::pressed, this, &Keypad::keyPressed);
        m_buttons << b;
        if (d.id == "shift") m_shift = b;
        if (d.id == "alpha") m_alpha = b;
        bool numberBlock = d.style == KeyStyle::Number || d.style == KeyStyle::Accent;
        if (!numberBlock) {
            int row, col;
            if (topIndex < 8) {   // first two rows have 4 keys around the pad
                row = topIndex / 4;
                int c = topIndex % 4;
                col = c < 2 ? c : c + 2;
            } else {
                row = 2 + (topIndex - 8) / 6;
                col = (topIndex - 8) % 6;
            }
            top->addWidget(b, row, col);
            ++topIndex;
        } else {
            bottom->addWidget(b, bottomIndex / 5, bottomIndex % 5);
            ++bottomIndex;
        }
    }
    outer->addLayout(top, 5);
    outer->addLayout(bottom, 5);
}

void Keypad::setLayer(KeyLayer l)
{
    for (auto *b : m_buttons) b->setLayer(l);
}

void Keypad::setShiftLatched(bool b)
{
    if (m_shift) m_shift->setLatched(b);
}

void Keypad::setAlphaLatched(bool b)
{
    if (m_alpha) m_alpha->setLatched(b);
}
