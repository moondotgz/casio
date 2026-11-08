#include "equationpage.h"
#include "core/numeric.h"
#include "richtext.h"
#include <QApplication>
#include <QComboBox>
#include <QHeaderView>
#include <QLabel>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>

// ------------------------------------------------------------------ CoefficientPage

QTableWidget *CoefficientPage::makeGrid(QWidget *parent)
{
    m_coef = new QTableWidget(parent);
    m_coef->setItemDelegate(new ExprDelegate(m_coef));
    m_coef->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_coef->verticalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_coef->setEditTriggers(QAbstractItemView::AllEditTriggers);
    return m_coef;
}

void CoefficientPage::setGrid(int rows, const QStringList &headers)
{
    m_coef->clear();
    m_coef->setRowCount(rows);
    m_coef->setColumnCount(int(headers.size()));
    m_coef->setHorizontalHeaderLabels(headers);
    for (int r = 0; r < rows; ++r)
        for (int c = 0; c < headers.size(); ++c) m_coef->setItem(r, c, new QTableWidgetItem("0"));
    m_coef->setCurrentCell(0, 0);
}

std::vector<std::vector<double>> CoefficientPage::readGrid()
{
    flushTableEditor(m_coef);
    std::vector<std::vector<double>> out;
    for (int r = 0; r < m_coef->rowCount(); ++r) {
        std::vector<double> row;
        for (int c = 0; c < m_coef->columnCount(); ++c) {
            QString t = m_coef->item(r, c) ? m_coef->item(r, c)->text().trimmed() : QString();
            Value v = evalString(t.isEmpty() ? "0" : t);
            if (!v.isReal()) throw CalcError(ErrKind::Math);
            row.push_back(v.re());
        }
        out.push_back(row);
    }
    return out;
}

void CoefficientPage::insertText(const QString &text, bool startsNew)
{
    if (m_coef && m_coef->isVisible()) beginCellInput(m_coef);
    ModePage::insertText(text, startsNew);
}

void CoefficientPage::navigate(const QString &dir)
{
    if (!m_coef || !m_coef->isVisible()) { ModePage::navigate(dir); return; }
    navigateTable(m_coef, dir);
}

// ------------------------------------------------------------------ EquationPage

EquationPage::EquationPage(AppState *st, QWidget *parent) : CoefficientPage(st, parent)
{
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    m_stack = new QStackedWidget(this);
    v->addWidget(m_stack);

    auto *input = new QWidget;
    auto *iv = new QVBoxLayout(input);
    iv->setContentsMargins(6, 4, 6, 6);
    m_type = new QComboBox(input);
    m_type->addItems({"Simul Equation: 2 unknowns", "Simul Equation: 3 unknowns", "Simul Equation: 4 unknowns",
                      "Polynomial: ax²+bx+c", "Polynomial: ax³+bx²+cx+d",
                      "Polynomial: ax⁴+bx³+cx²+dx+e"});
    m_type->setFocusPolicy(Qt::NoFocus);
    iv->addWidget(m_type);
    iv->addWidget(makeGrid(input), 1);
    m_error = new QLabel(input);
    m_error->setObjectName("errorBox");
    m_error->hide();
    iv->addWidget(m_error);
    auto *hint = new QLabel("Type coefficients (expressions allowed).   [=] : Solve", input);
    hint->setObjectName("hint");
    iv->addWidget(hint);
    m_stack->addWidget(input);

    auto *res = new QWidget;
    auto *rv = new QVBoxLayout(res);
    rv->setContentsMargins(8, 6, 8, 6);
    m_resTitle = lcdLabel("", res, true);
    rv->addWidget(m_resTitle);
    m_result = new MathView(res);
    m_result->setAlignment(Qt::AlignLeft | Qt::AlignTop);
    rv->addWidget(m_result, 1);
    auto *h2 = new QLabel("[S⇔D] : Exact / Decimal      [AC] : Edit coefficients", res);
    h2->setObjectName("hint");
    rv->addWidget(h2);
    m_stack->addWidget(res);

    connect(m_type, &QComboBox::currentIndexChanged, this, [this] { setupGrid(); });
    setupGrid();
}

void EquationPage::setupGrid()
{
    int t = m_type->currentIndex();
    if (t < 3) {
        int n = t + 2;
        QStringList h;
        const char *letters = "abcde";
        for (int i = 0; i <= n; ++i) h << QString(QChar(letters[i]));
        setGrid(n, h);
        QStringList vh;
        for (int i = 1; i <= n; ++i) vh << QString::number(i);
        m_coef->setVerticalHeaderLabels(vh);
    } else {
        int deg = t - 1;
        QStringList h;
        const char *letters = "abcde";
        for (int i = 0; i <= deg; ++i) h << QString(QChar(letters[i]));
        setGrid(1, h);
        m_coef->setItem(0, 0, new QTableWidgetItem("1"));
    }
    m_error->hide();
}

void EquationPage::activated()
{
    if (m_stack->currentIndex() == 0) {
        m_coef->setFocus();
        if (m_coef->currentRow() < 0) m_coef->setCurrentCell(0, 0);
    }
}

void EquationPage::settingsChanged()
{
    if (m_stack->currentIndex() == 1) solve();
}

void EquationPage::solve()
{
    m_error->hide();
    const int t = m_type->currentIndex();
    MathStyle ms = mathStyle(19);
    DisplayOpts o;
    o.approx = m_approx;
    std::vector<BoxPtr> lines;
    try {
        auto g = readGrid();
        if (t < 3) {
            int n = t + 2;
            std::vector<std::vector<double>> A(static_cast<size_t>(n), std::vector<double>(static_cast<size_t>(n)));
            std::vector<double> b(static_cast<size_t>(n)), x;
            for (int r = 0; r < n; ++r) {
                for (int c = 0; c < n; ++c) A[size_t(r)][size_t(c)] = g[size_t(r)][size_t(c)];
                b[size_t(r)] = g[size_t(r)][size_t(n)];
            }
            auto status = num::solveLinear(A, b, x);
            m_resTitle->setText(fontSafe(m_type->currentText(), m_resTitle->font()));
            if (status == num::LinStatus::NoSolution) {
                lines.push_back(layoutText("No Solution", ms));
            } else if (status == num::LinStatus::Infinite) {
                lines.push_back(layoutText("Infinite Solution", ms));
            } else {
                const QStringList names = {"x", "y", "z", "t"};
                for (int i = 0; i < n; ++i) {
                    lines.push_back(layoutLabeled(names[i], layoutValue(Value(x[size_t(i)]), settings(), o, ms), ms));
                }
            }
        } else {
            std::vector<double> coeffs = g[0];
            if (coeffs[0] == 0) throw CalcError(ErrKind::Math);
            auto roots = num::polyRoots(coeffs);
            m_resTitle->setText(fontSafe(m_type->currentText(), m_resTitle->font()));
            const QStringList subs = {"₁", "₂", "₃", "₄"};
            bool allEqual = true;
            for (const auto &r : roots) allEqual = allEqual && r == roots[0];
            if (allEqual && roots.size() > 1) {
                lines.push_back(layoutLabeled("x", layoutValue(Value(roots[0]), settings(), o, ms), ms));
            } else {
                for (size_t i = 0; i < roots.size(); ++i)
                    lines.push_back(layoutLabeled("x" + subs[int(i)], layoutValue(Value(roots[i]), settings(), o, ms), ms));
            }
            if (coeffs.size() == 3) {   // quadratic: vertex
                double a = coeffs[0], b = coeffs[1], c = coeffs[2];
                double xv = -b / (2 * a), yv = c - b * b / (4 * a);
                QString kind = a > 0 ? "Minimum" : "Maximum";
                lines.push_back(layoutText(" ", ms, 0.5));
                lines.push_back(layoutLabeled("x-Value " + kind, layoutValue(Value(xv), settings(), o, ms), ms));
                lines.push_back(layoutLabeled("y-Value " + kind, layoutValue(Value(yv), settings(), o, ms), ms));
            }
        }
    } catch (const CalcError &e) {
        m_error->setText(errorName(e.kind));
        m_error->show();
        m_stack->setCurrentIndex(0);
        return;
    }
    m_result->setBox(layoutLines(lines, false, 6));
    m_stack->setCurrentIndex(1);
}

void EquationPage::execute(bool approx)
{
    if (m_stack->currentIndex() == 1) return;
    m_approx = approx;
    solve();
}

void EquationPage::allClear()
{
    if (m_stack->currentIndex() == 1) {
        m_stack->setCurrentIndex(0);
        m_coef->setFocus();
        return;
    }
    ModePage::allClear();
}

QList<MenuItem> EquationPage::optnMenu() const
{
    QList<MenuItem> types;
    for (int i = 0; i < m_type->count(); ++i) types.push_back({m_type->itemText(i), QString("c:TYPE:%1").arg(i)});
    return {{"Select Type", {}, types}};
}

void EquationPage::command(const QString &cmd)
{
    if (cmd.startsWith("TYPE:")) {
        m_type->setCurrentIndex(cmd.mid(5).toInt());
        m_stack->setCurrentIndex(0);
        m_coef->setFocus();
        return;
    }
    if (cmd == "SD" && m_stack->currentIndex() == 1) {
        m_approx = !m_approx;
        solve();
    }
}
