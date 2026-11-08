#include "tablepage.h"
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QStackedWidget>
#include <QTableWidget>
#include <cmath>

TablePage::TablePage(AppState *st, QWidget *parent) : ModePage(st, parent)
{
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    m_stack = new QStackedWidget(this);
    v->addWidget(m_stack);

    auto *form = new QWidget;
    auto *fv = new QVBoxLayout(form);
    fv->setContentsMargins(10, 8, 10, 8);
    fv->addWidget(lcdLabel("Define functions and range", form, true));
    auto *fl = new QFormLayout;
    m_f = new ExprEdit(form);
    m_g = new ExprEdit(form);
    m_start = new ExprEdit(form);
    m_end = new ExprEdit(form);
    m_step = new ExprEdit(form);
    m_f->setText("x²");
    m_g->setPlaceholderText("optional");
    m_start->setText("1");
    m_end->setText("5");
    m_step->setText("1");
    fl->addRow(lcdLabel("f(x) =", form), m_f);
    fl->addRow(lcdLabel("g(x) =", form), m_g);
    fl->addRow(lcdLabel("Start :", form), m_start);
    fl->addRow(lcdLabel("End :", form), m_end);
    fl->addRow(lcdLabel("Step :", form), m_step);
    fv->addLayout(fl);
    m_error = new QLabel(form);
    m_error->setObjectName("errorBox");
    m_error->hide();
    fv->addWidget(m_error);
    fv->addStretch();
    auto *hint = new QLabel("[=] : Make table      [▲][▼] : Move      max 45 rows (30 with g(x))", form);
    hint->setObjectName("hint");
    fv->addWidget(hint);
    m_stack->addWidget(form);

    auto *tab = new QWidget;
    auto *tv = new QVBoxLayout(tab);
    tv->setContentsMargins(6, 4, 6, 6);
    m_title = lcdLabel("", tab);
    tv->addWidget(m_title);
    m_table = new QTableWidget(0, 3, tab);
    m_table->setItemDelegate(new ExprDelegate(m_table));
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->verticalHeader()->setDefaultSectionSize(21);
    m_table->setEditTriggers(QAbstractItemView::DoubleClicked | QAbstractItemView::EditKeyPressed |
                             QAbstractItemView::AnyKeyPressed);
    tv->addWidget(m_table, 1);
    auto *h2 = new QLabel("Edit an x cell to recompute that row.      [AC] : Back to definition", tab);
    h2->setObjectName("hint");
    tv->addWidget(h2);
    m_stack->addWidget(tab);

    for (auto *e : fields())
        connect(e, &QLineEdit::returnPressed, this, [this, e] {
            auto f = fields();
            int i = int(f.indexOf(e));
            if (i + 1 < f.size()) f[i + 1]->setFocus();
            else execute(false);
        });
    connect(m_table, &QTableWidget::cellChanged, this, [this](int r, int c) {
        if (!m_updating && c == 0) computeRow(r);
    });
}

QList<ExprEdit *> TablePage::fields() const { return {m_f, m_g, m_start, m_end, m_step}; }

QWidget *TablePage::editor() const { return m_stack->currentIndex() == 0 ? m_f : nullptr; }

void TablePage::activated()
{
    if (m_stack->currentIndex() == 0) m_f->setFocus();
}

void TablePage::settingsChanged()
{
    if (m_stack->currentIndex() == 1) generate();
}

double TablePage::fieldValue(ExprEdit *e)
{
    Value v = evalString(e->text());
    if (!v.isReal()) throw CalcError(ErrKind::Math);
    return v.re();
}

QString TablePage::evalAt(const NodePtr &n, double x)
{
    try {
        prepareContext();
        Evaluator ev(ctx());
        return toDisplayText(formatDecimal(ev.evalRealAt(n, "x", x), settings()));
    } catch (const CalcError &e) {
        return "ERROR";
    }
}

void TablePage::computeRow(int row)
{
    auto *xi = m_table->item(row, 0);
    if (!xi) return;
    m_updating = true;
    try {
        Value v = evalString(xi->text());
        double x = v.re();
        xi->setText(toLinearText(formatDecimal(x, settings())));
        m_table->setItem(row, 1, new QTableWidgetItem(evalAt(m_fn, x)));
        if (m_gn) m_table->setItem(row, 2, new QTableWidgetItem(evalAt(m_gn, x)));
    } catch (const CalcError &e) {
        xi->setText("ERROR");
    }
    for (int c = 1; c < m_table->columnCount(); ++c)
        if (auto *it = m_table->item(row, c)) it->setFlags(it->flags() & ~Qt::ItemIsEditable);
    m_updating = false;
}

void TablePage::generate()
{
    m_error->hide();
    try {
        ParseOptions po;
        if (m_f->text().trimmed().isEmpty()) throw CalcError(ErrKind::Syntax);
        m_fn = parseExpression(m_f->text(), po);
        m_gn = m_g->text().trimmed().isEmpty() ? nullptr : parseExpression(m_g->text(), po);
    } catch (const CalcError &e) {
        m_error->setText(errorName(e.kind) + " in function");
        m_error->show();
        return;
    }
    double a, b, s;
    try {
        a = fieldValue(m_start);
        b = fieldValue(m_end);
        s = fieldValue(m_step);
    } catch (const CalcError &e) {
        m_error->setText(errorName(e.kind));
        m_error->show();
        return;
    }
    int limit = m_gn ? 30 : 45;
    double count = (s == 0) ? -1 : std::floor((b - a) / s + 1e-9) + 1;
    if (s == 0 || count < 1 || count > limit) {
        m_error->setText(QString("Range ERROR  (1 to %1 rows; check Start/End/Step)").arg(limit));
        m_error->show();
        return;
    }
    m_updating = true;
    m_table->clear();
    m_table->setColumnCount(m_gn ? 3 : 2);
    m_table->setRowCount(int(count));
    QStringList h = {"x", "f(x)"};
    if (m_gn) h << "g(x)";
    m_table->setHorizontalHeaderLabels(h);
    m_updating = false;
    for (int i = 0; i < int(count); ++i) {
        double x = a + i * s;
        if (std::abs(x) < 1e-13 * std::max(1.0, std::abs(s))) x = 0;
        m_updating = true;
        m_table->setItem(i, 0, new QTableWidgetItem(toLinearText(formatDecimal(x, settings()))));
        m_updating = false;
        computeRow(i);
    }
    m_title->setText("f(x) = " + m_f->text() + (m_gn ? "      g(x) = " + m_g->text() : QString()));
    m_stack->setCurrentIndex(1);
    m_table->setFocus();
    m_table->setCurrentCell(0, 0);
}

void TablePage::execute(bool approx)
{
    Q_UNUSED(approx);
    if (m_stack->currentIndex() == 0) generate();
    else flushTableEditor(m_table);
}

void TablePage::allClear()
{
    if (m_stack->currentIndex() == 1) {
        m_stack->setCurrentIndex(0);
        m_f->setFocus();
        return;
    }
    ModePage::allClear();
}

void TablePage::navigate(const QString &dir)
{
    if (m_stack->currentIndex() == 0 && (dir == "UP" || dir == "DOWN")) {
        if (dir == "UP") focusPreviousChild();
        else focusNextChild();
        return;
    }
    ModePage::navigate(dir);
}
