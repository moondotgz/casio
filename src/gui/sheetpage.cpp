#include "sheetpage.h"
#include "theme.h"
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QPushButton>
#include <QRegularExpression>
#include <QStackedWidget>
#include <QTableWidget>

SheetPage::SheetPage(AppState *st, QWidget *parent) : ModePage(st, parent)
{
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    m_stack = new QStackedWidget(this);
    v->addWidget(m_stack);

    // sheet view
    auto *sheet = new QWidget;
    auto *sv = new QVBoxLayout(sheet);
    sv->setContentsMargins(6, 4, 6, 6);
    m_table = new QTableWidget(Rows, Cols, sheet);
    m_table->setItemDelegate(new RichDelegate(m_table));
    m_table->setHorizontalHeaderLabels({"A", "B", "C", "D", "E"});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->verticalHeader()->setDefaultSectionSize(21);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->setSelectionMode(QAbstractItemView::SingleSelection);
    m_table->setFocusPolicy(Qt::ClickFocus);
    for (int r = 0; r < Rows; ++r)
        for (int c = 0; c < Cols; ++c) {
            auto *it = new QTableWidgetItem;
            it->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
            m_table->setItem(r, c, it);
        }
    sv->addWidget(m_table, 1);
    m_error = new QLabel(sheet);
    m_error->setObjectName("errorBox");
    m_error->hide();
    sv->addWidget(m_error);
    auto *row = new QHBoxLayout;
    m_cellLabel = lcdLabel("A1 =", sheet, true);
    m_cellLabel->setMinimumWidth(44);
    m_edit = new ExprEdit(sheet);
    m_edit->setPlaceholderText("value, or =formula  (e.g. =A1×2, =Sum(A1:A5))");
    row->addWidget(m_cellLabel);
    row->addWidget(m_edit, 1);
    sv->addLayout(row);
    m_stack->addWidget(sheet);

    // fill panel
    auto *fill = new QWidget;
    auto *fv = new QVBoxLayout(fill);
    fv->setContentsMargins(10, 8, 10, 8);
    m_fillTitle = lcdLabel("Fill Formula", fill, true);
    fv->addWidget(m_fillTitle);
    auto *form = new QFormLayout;
    m_fillExpr = new ExprEdit(fill);
    m_fillRange = new ExprEdit(fill);
    form->addRow(lcdLabel("Form =", fill), m_fillExpr);
    form->addRow(lcdLabel("Range :", fill), m_fillRange);
    fv->addLayout(form);
    auto *fh = new QLabel("Relative references shift for each cell; use $ for absolute (e.g. =$A$1+A1).\n"
                          "[=] : Confirm      [AC] : Cancel", fill);
    fh->setObjectName("hint");
    fh->setWordWrap(true);
    fv->addWidget(fh);
    fv->addStretch();
    m_stack->addWidget(fill);

    connect(m_table, &QTableWidget::currentCellChanged, this, [this] { loadCurrent(); });
    connect(m_edit, &QLineEdit::returnPressed, this, [this] { execute(false); });
    connect(m_edit, &ExprEdit::upPressed, this, [this] { navigate("UP"); });
    connect(m_edit, &ExprEdit::downPressed, this, [this] { navigate("DOWN"); });
    connect(m_fillExpr, &QLineEdit::returnPressed, this, [this] { m_fillRange->setFocus(); });
    connect(m_fillRange, &QLineEdit::returnPressed, this, [this] { finishFill(); });
    m_table->setCurrentCell(0, 0);
}

QString SheetPage::cellName(int r, int c) { return QString(QChar('A' + c)) + QString::number(r + 1); }

bool SheetPage::parseCell(const QString &s, int &r, int &c)
{
    static const QRegularExpression re("^\\$?([A-E])\\$?([0-9]{1,2})$");
    auto m = re.match(s.trimmed());
    if (!m.hasMatch()) return false;
    c = m.captured(1)[0].unicode() - 'A';
    r = m.captured(2).toInt() - 1;
    return r >= 0 && r < Rows && c >= 0 && c < Cols;
}

QString SheetPage::currentCell() const
{
    int r = std::max(0, m_table->currentRow()), c = std::max(0, m_table->currentColumn());
    return cellName(r, c);
}

QWidget *SheetPage::editor() const { return m_stack->currentIndex() == 0 ? m_edit : m_fillExpr; }

void SheetPage::activated()
{
    m_edit->setFocus();
    loadCurrent();
}

void SheetPage::loadCurrent()
{
    QString cell = currentCell();
    m_cellLabel->setText(cell + " =");
    m_edit->setText(m_raw.value(cell));
    m_error->hide();
}

void SheetPage::commit(const QString &cell, const QString &text)
{
    QString t = text.trimmed();
    if (t.isEmpty()) {
        m_raw.remove(cell);
    } else if (t.startsWith('=')) {
        m_raw[cell] = t;
    } else {
        // constants are evaluated once when entered
        Value v = evalFormula(t);
        Settings s = settings();
        s.format = NumFormat::Norm1;
        m_raw[cell] = toLinearText(formatComplex(v.z, s, DisplayOpts{true}));
        m_raw[cell].replace(QStringLiteral("×10^"), QStringLiteral("ᴇ"));
    }
    if (settings().sheetAutoCalc) recalc();
    else refreshDisplay();
}

Value SheetPage::evalFormula(const QString &expr)
{
    struct Depth {
        int &d;
        explicit Depth(int &x) : d(x) { ++d; }
        ~Depth() { --d; }
    } depth(m_depth);
    if (m_depth == 1) {
        // install the hooks once for the outermost evaluation; nested cell lookups reuse them
        prepareContext();
        m_visiting.clear();
        ctx().cellHook = [this](const QString &cell) { return evalCell(cell); };
        ctx().rangeHook = [this](const QString &range) {
            QStringList ends = range.split(':');
            int r0, c0, r1, c1;
            if (ends.size() != 2 || !parseCell(ends[0], r0, c0) || !parseCell(ends[1], r1, c1))
                throw CalcError(ErrKind::Syntax);
            if (r0 > r1) std::swap(r0, r1);
            if (c0 > c1) std::swap(c0, c1);
            std::vector<cplx> vals;
            for (int r = r0; r <= r1; ++r)
                for (int c = c0; c <= c1; ++c) {
                    QString n = cellName(r, c);
                    if (!m_raw.contains(n)) continue;   // empty cells are ignored
                    vals.push_back(evalCell(n).z);
                }
            if (vals.empty()) throw CalcError(ErrKind::Math);
            Value v = Value::vector(int(vals.size()));
            v.m = vals;
            return v;
        };
    }
    ParseOptions po;
    po.lex.cellRefs = true;
    Evaluator ev(ctx());
    Value v = ev.evaluate(parseExpression(expr, po));
    if (!v.isReal()) throw CalcError(ErrKind::Math);
    return v;
}

Value SheetPage::evalCell(const QString &name)
{
    if (m_val.contains(name)) return m_val[name];
    if (m_err.contains(name)) throw CalcError(m_err[name]);
    if (!m_raw.contains(name)) return Value(0.0);
    if (m_visiting.contains(name)) throw CalcError(ErrKind::Circular);
    m_visiting.insert(name);
    QString raw = m_raw[name];
    try {
        Value v = evalFormula(raw.startsWith('=') ? raw.mid(1) : raw);
        m_visiting.remove(name);
        m_val[name] = v;
        return v;
    } catch (const CalcError &e) {
        m_visiting.remove(name);
        m_err[name] = e.kind;
        throw;
    }
}

void SheetPage::recalc()
{
    m_val.clear();
    m_err.clear();
    for (auto it = m_raw.begin(); it != m_raw.end(); ++it) {
        try {
            evalCell(it.key());
        } catch (const CalcError &) {
        }
    }
    refreshDisplay();
}

void SheetPage::refreshDisplay()
{
    for (int r = 0; r < Rows; ++r)
        for (int c = 0; c < Cols; ++c) {
            QString n = cellName(r, c);
            QTableWidgetItem *it = m_table->item(r, c);
            if (!m_raw.contains(n)) {
                it->setText({});
                continue;
            }
            bool formula = m_raw[n].startsWith('=');
            QFont f = it->font();
            f.setItalic(formula);
            it->setFont(f);
            it->setToolTip(m_raw[n]);
            if (m_err.contains(n)) {
                it->setText("ERROR");
                it->setForeground(themeColor("lcdError"));
            } else if (m_val.contains(n)) {
                it->setText(toDisplayText(formatDecimal(m_val[n].re(), settings())));
                if (formula) it->setForeground(themeColor("lcdAccent"));
                else it->setData(Qt::ForegroundRole, QVariant());
            } else {
                it->setText(formula ? "(recalc)" : m_raw[n]);
            }
        }
}

void SheetPage::execute(bool approx)
{
    Q_UNUSED(approx);
    if (m_stack->currentIndex() == 1) { finishFill(); return; }
    QString cell = currentCell();
    try {
        commit(cell, m_edit->text());
        m_error->hide();
        int r = m_table->currentRow();
        if (r + 1 < Rows) m_table->setCurrentCell(r + 1, m_table->currentColumn());
        else loadCurrent();
        if (m_err.contains(cell)) {
            m_error->setText(cell + ": " + errorName(m_err[cell]));
            m_error->show();
        }
    } catch (const CalcError &e) {
        m_error->setText(errorName(e.kind));
        m_error->show();
        if (e.pos >= 0) m_edit->setCursorPosition(e.pos);
    }
    m_edit->setFocus();
}

void SheetPage::allClear()
{
    if (m_stack->currentIndex() == 1) {
        m_stack->setCurrentIndex(0);
        m_edit->setFocus();
        return;
    }
    if (m_edit->text() != m_raw.value(currentCell())) loadCurrent();
    else m_edit->clear();
    m_error->hide();
}

void SheetPage::navigate(const QString &dir)
{
    if (m_stack->currentIndex() == 1) {
        if (dir == "UP") m_fillExpr->setFocus();
        else if (dir == "DOWN") m_fillRange->setFocus();
        else ModePage::navigate(dir);
        return;
    }
    bool unchanged = m_edit->text() == m_raw.value(currentCell());
    int r = m_table->currentRow(), c = m_table->currentColumn();
    if (dir == "UP" && r > 0) m_table->setCurrentCell(r - 1, c);
    else if (dir == "DOWN" && r + 1 < Rows) m_table->setCurrentCell(r + 1, c);
    else if ((dir == "LEFT" || dir == "RIGHT") && unchanged) {
        int nc = c + (dir == "LEFT" ? -1 : 1);
        if (nc >= 0 && nc < Cols) m_table->setCurrentCell(r, nc);
    } else if (dir == "LEFT" || dir == "RIGHT") {
        ModePage::navigate(dir);
    }
    m_edit->setFocus();
}

QString SheetPage::shiftRefs(const QString &formula, int dr, int dc, bool &ok)
{
    static const QRegularExpression re("(\\$?)([A-E])(\\$?)([0-9]{1,2})");
    QString out;
    qsizetype last = 0;
    ok = true;
    auto it = re.globalMatch(formula);
    while (it.hasNext()) {
        auto m = it.next();
        // skip letters that are part of a word (e.g. "Mean")
        qsizetype s = m.capturedStart();
        if (s > 0 && formula[s - 1].isLetter()) continue;
        out += formula.mid(last, s - last);
        int col = m.captured(2)[0].unicode() - 'A';
        int row = m.captured(4).toInt();
        if (m.captured(1).isEmpty()) col += dc;
        if (m.captured(3).isEmpty()) row += dr;
        if (col < 0 || col >= Cols || row < 1 || row > Rows) ok = false;
        out += m.captured(1) + QChar('A' + std::clamp(col, 0, Cols - 1)) + m.captured(3) + QString::number(row);
        last = m.capturedEnd();
    }
    out += formula.mid(last);
    return out;
}

void SheetPage::startFill(bool formula)
{
    m_fillFormula = formula;
    m_fillTitle->setText(formula ? "Fill Formula" : "Fill Value");
    auto *lab = qobject_cast<QFormLayout *>(m_fillExpr->parentWidget()->layout()->itemAt(1)->layout());
    if (lab) {
        if (auto *l = qobject_cast<QLabel *>(lab->itemAt(0, QFormLayout::LabelRole)->widget()))
            l->setText(formula ? "Form =" : "Value :");
    }
    QString cell = currentCell();
    m_fillExpr->setText(formula ? (m_raw.value(cell).startsWith('=') ? m_raw.value(cell) : "=") : m_raw.value(cell));
    m_fillRange->setText(cell + ":" + cell);
    m_stack->setCurrentIndex(1);
    m_fillExpr->setFocus();
}

void SheetPage::finishFill()
{
    QStringList ends = m_fillRange->text().split(':');
    int r0, c0, r1, c1;
    if (ends.size() == 1) ends << ends[0];
    if (ends.size() != 2 || !parseCell(ends[0], r0, c0) || !parseCell(ends[1], r1, c1)) {
        emit message("Range ERROR: use a range such as A1:A5");
        return;
    }
    if (r0 > r1) std::swap(r0, r1);
    if (c0 > c1) std::swap(c0, c1);
    QString base = m_fillExpr->text().trimmed();
    try {
        if (m_fillFormula) {
            if (!base.startsWith('=')) base = "=" + base;
            for (int r = r0; r <= r1; ++r)
                for (int c = c0; c <= c1; ++c) {
                    bool ok;
                    QString f = shiftRefs(base, r - r0, c - c0, ok);
                    if (!ok) throw CalcError(ErrKind::Range);
                    m_raw[cellName(r, c)] = f;
                }
        } else {
            m_val.clear();
            m_err.clear();
            Value v = evalFormula(base.startsWith('=') ? base.mid(1) : base);
            QString txt = toLinearText(formatComplex(v.z, settings(), DisplayOpts{true}));
            txt.replace(QStringLiteral("×10^"), QStringLiteral("ᴇ"));
            for (int r = r0; r <= r1; ++r)
                for (int c = c0; c <= c1; ++c) m_raw[cellName(r, c)] = txt;
        }
    } catch (const CalcError &e) {
        emit message(errorName(e.kind));
        return;
    }
    recalc();
    m_stack->setCurrentIndex(0);
    loadCurrent();
    m_edit->setFocus();
}

void SheetPage::command(const QString &cmd)
{
    if (cmd == "FILLF") startFill(true);
    else if (cmd == "FILLV") startFill(false);
    else if (cmd == "RECALC") { recalc(); emit message("Recalculated"); }
    else if (cmd == "DELALL") { m_raw.clear(); recalc(); loadCurrent(); }
    else if (cmd == "DELCELL") { m_raw.remove(currentCell()); recalc(); loadCurrent(); }
    else if (cmd == "FREE") emit message(QString("Free space: %1 of %2 cells").arg(Rows * Cols - m_raw.size()).arg(Rows * Cols));
}

QList<MenuItem> SheetPage::optnMenu() const
{
    return {{"Fill Formula", "c:FILLF"},
            {"Fill Value", "c:FILLV"},
            {"Functions", {}, {{"Min", "i:Min(§)"}, {"Max", "i:Max(§)"}, {"Mean", "i:Mean(§)"},
                               {"Sum", "i:Sum(§)"}, {"$  (absolute ref)", "i:$"}, {":  (range)", "i::"},
                               {"=  (formula)", "i:="}}},
            {"Recalculate", "c:RECALC"},
            {"Delete Cell", "c:DELCELL"},
            {"Delete All", "c:DELALL"},
            {"Free Space", "c:FREE"},
            {"Numeric / Other", {}, calcMenuItems()}};
}
