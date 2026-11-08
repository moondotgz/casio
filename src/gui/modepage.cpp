#include "modepage.h"
#include "core/constants.h"
#include "core/lexer.h"
#include "theme.h"
#include <QApplication>
#include <QKeyEvent>
#include <QLabel>
#include <QMetaMethod>
#include <QTableWidget>

// ------------------------------------------------------------------ ExprEdit

ExprEdit::ExprEdit(QWidget *parent) : QLineEdit(parent)
{
    setObjectName("exprEdit");
#ifdef Q_OS_IOS
    // Input comes from the on-screen calculator keypad; keep the iOS system keyboard from covering it.
    setAttribute(Qt::WA_InputMethodEnabled, false);
#endif
}

void ExprEdit::insertTemplate(const QString &t)
{
    QString s = t;
    int idx = int(s.indexOf(QChar(0x00A7)));
    s.remove(QChar(0x00A7));
    if (hasSelectedText()) del();
    int pos = cursorPosition();
    insert(s);
    if (idx >= 0) setCursorPosition(pos + idx);
}

void ExprEdit::smartBackspace()
{
    if (hasSelectedText()) { del(); return; }
    int pos = cursorPosition();
    if (pos == 0) return;
    QString before = text().left(pos);
    int remove = 1;
    if (before.endsWith(QChar(0x203A))) {   // ‹constant›
        int open = int(before.lastIndexOf(QChar(0x2039)));
        if (open >= 0) remove = pos - open;
    } else {
        for (const QString &kw : allKeywords()) {
            if (before.endsWith(kw + "(")) { remove = int(kw.size()) + 1; break; }
            if (before.endsWith(kw)) { remove = int(kw.size()); break; }
        }
    }
    setSelection(pos - remove, remove);
    del();
}

void ExprEdit::keyPressEvent(QKeyEvent *e)
{
    switch (e->key()) {
    case Qt::Key_Up:
        if (isSignalConnected(QMetaMethod::fromSignal(&ExprEdit::upPressed))) { emit upPressed(); return; }
        e->ignore();   // let a parent (e.g. a table being edited) handle it
        return;
    case Qt::Key_Down:
        if (isSignalConnected(QMetaMethod::fromSignal(&ExprEdit::downPressed))) { emit downPressed(); return; }
        e->ignore();
        return;
    case Qt::Key_Backspace:
        if (e->modifiers() == Qt::NoModifier) { fresh = false; smartBackspace(); return; }
        break;
    case Qt::Key_Left: case Qt::Key_Right: case Qt::Key_Home: case Qt::Key_End: case Qt::Key_Delete:
        fresh = false;
        break;
    default:
        break;
    }
    const QString t = e->text();
    if (fresh && !t.isEmpty() && t[0].isPrint() && !(e->modifiers() & (Qt::ControlModifier | Qt::AltModifier))) {
        emit typedWhileFresh(t);
        return;
    }
    QLineEdit::keyPressEvent(e);
}

QWidget *ExprDelegate::createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &) const
{
    auto *e = new ExprEdit(parent);
    e->setFrame(false);
    e->setStyleSheet(QString("QLineEdit { border: 1px solid %1; border-radius: 0; padding: 0 2px; }")
                         .arg(themeColor("lcdAccent").name()));
    return e;
}

void flushTableEditor(QTableWidget *t)
{
    QWidget *f = QApplication::focusWidget();
    if (f && t->isAncestorOf(f) && f != t) {
        t->setFocus();   // focus-out makes the delegate commit and close the editor
        QApplication::processEvents();
    }
}

void beginCellInput(QTableWidget *t)
{
    auto *e = qobject_cast<ExprEdit *>(QApplication::focusWidget());
    if (e && t->isAncestorOf(e)) return;
    if (t->currentRow() < 0 || t->currentColumn() < 0) t->setCurrentCell(0, 0);
    QTableWidgetItem *it = t->currentItem();
    if (!it) {
        it = new QTableWidgetItem;
        t->setItem(t->currentRow(), t->currentColumn(), it);
    }
    t->editItem(it);
    if (auto *ed = qobject_cast<ExprEdit *>(QApplication::focusWidget())) ed->selectAll();
}

void navigateTable(QTableWidget *t, const QString &dir)
{
    flushTableEditor(t);
    int r = std::max(0, t->currentRow()), c = std::max(0, t->currentColumn());
    if (dir == "LEFT") c = std::max(0, c - 1);
    if (dir == "RIGHT") c = std::min(t->columnCount() - 1, c + 1);
    if (dir == "UP") r = std::max(0, r - 1);
    if (dir == "DOWN") r = std::min(t->rowCount() - 1, r + 1);
    t->setCurrentCell(r, c);
    t->setFocus();
}

// ------------------------------------------------------------------ ModePage

ModePage::ModePage(AppState *s, QWidget *parent) : QWidget(parent), st(s) {}

MathStyle ModePage::mathStyle(qreal size) const
{
    MathStyle s;
    s.size = size * appearance().mathScale;
    s.settings = &st->ctx.settings;
    s.natural = settings().naturalInput();
    return s;
}

ExprEdit *ModePage::focusedEdit() const
{
    auto *w = qobject_cast<ExprEdit *>(QApplication::focusWidget());
    if (w && isAncestorOf(w)) return w;
    return qobject_cast<ExprEdit *>(editor());
}

void ModePage::insertText(const QString &text, bool startsNew)
{
    Q_UNUSED(startsNew);
    if (auto *e = focusedEdit()) {
        e->insertTemplate(text);
        e->setFocus();
    }
}

void ModePage::allClear()
{
    if (auto *e = focusedEdit()) e->clear();
}

void ModePage::del()
{
    if (auto *e = focusedEdit()) e->smartBackspace();
}

void ModePage::navigate(const QString &dir)
{
    QWidget *w = QApplication::focusWidget();
    if (!w || !isAncestorOf(w)) w = editor();
    if (!w) return;
    int key = dir == "LEFT" ? Qt::Key_Left : dir == "RIGHT" ? Qt::Key_Right : dir == "UP" ? Qt::Key_Up : Qt::Key_Down;
    if (auto *e = qobject_cast<ExprEdit *>(w)) {
        if (key == Qt::Key_Left) { e->fresh = false; e->cursorBackward(false); return; }
        if (key == Qt::Key_Right) { e->fresh = false; e->cursorForward(false); return; }
    }
    QKeyEvent press(QEvent::KeyPress, key, Qt::NoModifier);
    QApplication::sendEvent(w, &press);
}

void ModePage::prepareContext()
{
    Context &c = ctx();
    c.complexMode = false;
    c.base = 0;
    c.varHook = nullptr;
    c.funcHook = nullptr;
    c.cellHook = nullptr;
    c.rangeHook = nullptr;
}

Value ModePage::evalString(const QString &src, bool allowMulti)
{
    prepareContext();
    ParseOptions po;
    po.lex.base = ctx().base;
    if (src.trimmed().isEmpty()) throw CalcError(ErrKind::Syntax, 0);
    Evaluator ev(ctx());
    if (allowMulti) {
        Value v;
        for (const auto &s : parseStatements(src, po)) v = ev.evaluate(s);
        return v;
    }
    return ev.evaluate(parseExpression(src, po));
}

void ModePage::showError(QLabel *label, const CalcError &e)
{
    label->setText(errorName(e.kind));
    label->show();
}

QLabel *ModePage::lcdLabel(const QString &text, QWidget *parent, bool bold)
{
    auto *l = new QLabel(text, parent);
    l->setObjectName(bold ? "lcdTitle" : "lcdText");
    return l;
}

QList<MenuItem> ModePage::optnMenu() const { return calcMenuItems(); }

QList<MenuItem> hyperbolicMenu()
{
    const QString inv = QStringLiteral("\u207B\u00B9");
    return {{"sinh", "i:sinh("}, {"cosh", "i:cosh("}, {"tanh", "i:tanh("},
            {"sinh" + inv, "i:sinh" + inv + "("}, {"cosh" + inv, "i:cosh" + inv + "("}, {"tanh" + inv, "i:tanh" + inv + "("}};
}

QList<MenuItem> angleUnitMenu()
{
    return {{"\u00B0  (degrees)", "i:\u00B0"}, {"\u02B3  (radians)", "i:\u02B3"}, {"\u1D4D  (gradians)", "i:\u1D4D"},
            {"\u00B0 ' \"  (sexagesimal minutes)", "i:'"}};
}

QList<MenuItem> numericMenu()
{
    return {{"\u222B  Integration", "i:\u222B(\u00A7,,)"},
            {"d/dx  Differential", "i:d/dx(\u00A7,)"},
            {"\u03A3  Summation", "i:\u03A3(\u00A7,,)"},
            {"\u03A0  Product", "i:\u03A0(\u00A7,,)"},
            {"log\u2090b", "i:log(\u00A7,)"},
            {"Abs  |x|", "i:Abs(\u00A7)"},
            {"\u207F\u221A  n-th root", "i:root(\u00A7,)"}};
}

QList<MenuItem> ModePage::calcMenuItems() const
{
    return {
        {"Hyperbolic Func", {}, hyperbolicMenu()},
        {"Angle Unit", {}, angleUnitMenu()},
        {"Numeric Calc", {}, numericMenu()},
        {"Integer / Number", {}, {
            {"\u00F7R  (quotient & remainder)", "i:\u00F7R"},
            {"GCD", "i:GCD(\u00A7,)"}, {"LCM", "i:LCM(\u00A7,)"},
            {"Int  (integer part)", "i:Int(\u00A7)"}, {"Intg  (floor)", "i:Intg(\u00A7)"},
            {"Rnd  (round to display)", "i:Rnd(\u00A7)"},
            {"nPr", "i:nPr"}, {"nCr", "i:nCr"}, {"x!", "i:!"}}},
        {"Probability / Random", {}, {
            {"Ran#", "i:Ran#"}, {"RanInt#", "i:RanInt#(\u00A7,)"}}},
        {"Coordinates", {}, {
            {"Pol(x,y)", "i:Pol(\u00A7,)"}, {"Rec(r,\u03B8)", "i:Rec(\u00A7,)"}}},
        {"Variables", {}, {
            {"A", "i:A"}, {"B", "i:B"}, {"C", "i:C"}, {"D", "i:D"}, {"E", "i:E"}, {"F", "i:F"},
            {"x", "i:x"}, {"y", "i:y"}, {"M", "i:M"}, {"Ans", "i:Ans"}, {"PreAns", "i:PreAns"}}},
    };
}

QList<MenuItem> constantsMenu()
{
    QList<MenuItem> cats;
    int idx = 1;
    for (const auto &c : scientificConstants()) {
        if (cats.isEmpty() || cats.last().label != c.category) cats.push_back({c.category, {}, {}});
        QString label = QString("%1  %2   %3").arg(idx++, 2, 10, QChar('0')).arg(c.symbol, c.name);
        cats.last().children.push_back({label, "i:\u2039" + c.id + "\u203A"});
    }
    return cats;
}

QList<MenuItem> conversionsMenu()
{
    QList<MenuItem> cats;
    int idx = 1;
    for (const auto &c : unitConversions()) {
        if (cats.isEmpty() || cats.last().label != c.category) cats.push_back({c.category, {}, {}});
        QString label = QString("%1  %2").arg(idx++, 2, 10, QChar('0')).arg(c.id);
        cats.last().children.push_back({label, "i:\u2039" + c.id + "\u203A"});
    }
    return cats;
}

QList<MenuItem> catalogMenu()
{
    const QString inv = QStringLiteral("\u207B\u00B9");
    return {
        {"Trigonometric", {}, {{"sin", "i:sin("}, {"cos", "i:cos("}, {"tan", "i:tan("}, {"sin" + inv, "i:sin" + inv + "("},
                               {"cos" + inv, "i:cos" + inv + "("}, {"tan" + inv, "i:tan" + inv + "("}}},
        {"Hyperbolic", {}, hyperbolicMenu()},
        {"Exponent / Logarithm", {}, {{"log", "i:log("}, {"log\u2090b", "i:log(\u00A7,)"}, {"ln", "i:ln("},
                                      {"10^x", "i:10^("}, {"e^x", "i:e^("}, {"\u221A", "i:\u221A("},
                                      {"\u221B", "i:\u221B("}, {"\u207F\u221A", "i:root(\u00A7,)"}}},
        {"Numeric Calc", {}, numericMenu()},
        {"Angle Unit", {}, angleUnitMenu()},
        {"Complex", {}, {{"i", "i:i"}, {"Arg", "i:Arg("}, {"Conjg", "i:Conjg("}, {"ReP", "i:ReP("}, {"ImP", "i:ImP("},
                         {"\u25B6r\u2220\u03B8", "i:\u25B6r\u2220\u03B8"}, {"\u25B6a+bi", "i:\u25B6a+bi"}}},
        {"Matrix", {}, {{"Det", "i:Det("}, {"Trn", "i:Trn("}, {"Identity", "i:Identity("}, {"Ref", "i:Ref("},
                        {"Rref", "i:Rref("}, {"MatA", "i:MatA"}, {"MatB", "i:MatB"}, {"MatC", "i:MatC"},
                        {"MatD", "i:MatD"}, {"MatAns", "i:MatAns"}}},
        {"Vector", {}, {{"DotP", "i:DotP(\u00A7,)"}, {"CrossP", "i:CrossP(\u00A7,)"}, {"Angle", "i:Angle(\u00A7,)"},
                        {"UnitV", "i:UnitV("}, {"VctA", "i:VctA"}, {"VctB", "i:VctB"}, {"VctC", "i:VctC"},
                        {"VctD", "i:VctD"}, {"VctAns", "i:VctAns"}}},
        {"Other", {}, {{"Abs", "i:Abs("}, {"Int", "i:Int("}, {"Intg", "i:Intg("}, {"Rnd", "i:Rnd("},
                       {"GCD", "i:GCD(\u00A7,)"}, {"LCM", "i:LCM(\u00A7,)"}, {"\u00F7R", "i:\u00F7R"},
                       {"Ran#", "i:Ran#"}, {"RanInt#", "i:RanInt#(\u00A7,)"}, {"Pol", "i:Pol(\u00A7,)"},
                       {"Rec", "i:Rec(\u00A7,)"}, {"\u2192 (store)", "i:\u2192"}, {": (multi-statement)", "i::"}}},
        {"Scientific Constants", {}, constantsMenu()},
        {"Unit Conversions", {}, conversionsMenu()},
    };
}

QString ModePage::styleSheet() { return ThemeManager::instance().lcdStyleSheet(); }
