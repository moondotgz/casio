#include "inequalitypage.h"
#include "core/numeric.h"
#include "richtext.h"
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QLabel>
#include <QStackedWidget>
#include <QTableWidget>
#include <QVBoxLayout>
#include <cmath>

// ------------------------------------------------------------------ InequalityPage

InequalityPage::InequalityPage(AppState *st, QWidget *parent) : CoefficientPage(st, parent)
{
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(0, 0, 0, 0);
    m_stack = new QStackedWidget(this);
    v->addWidget(m_stack);

    auto *input = new QWidget;
    auto *iv = new QVBoxLayout(input);
    iv->setContentsMargins(6, 4, 6, 6);
    auto *row = new QHBoxLayout;
    m_degree = new QComboBox(input);
    m_degree->addItems({"ax²+bx+c", "ax³+bx²+cx+d", "ax⁴+bx³+cx²+dx+e"});
    m_rel = new QComboBox(input);
    m_rel->addItems({"> 0", "< 0", "≥ 0", "≤ 0"});
    for (auto *c : {m_degree, m_rel}) c->setFocusPolicy(Qt::NoFocus);
    row->addWidget(m_degree, 1);
    row->addWidget(m_rel);
    iv->addLayout(row);
    iv->addWidget(makeGrid(input), 1);
    m_error = new QLabel(input);
    m_error->setObjectName("errorBox");
    m_error->hide();
    iv->addWidget(m_error);
    auto *hint = new QLabel("[=] : Solve", input);
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

    connect(m_degree, &QComboBox::currentIndexChanged, this, [this] { setupGrid(); });
    setupGrid();
}

void InequalityPage::setupGrid()
{
    int deg = m_degree->currentIndex() + 2;
    QStringList h;
    const char *letters = "abcde";
    for (int i = 0; i <= deg; ++i) h << QString(QChar(letters[i]));
    setGrid(1, h);
    m_coef->setItem(0, 0, new QTableWidgetItem("1"));
}

void InequalityPage::activated()
{
    if (m_stack->currentIndex() == 0) m_coef->setFocus();
}

void InequalityPage::settingsChanged()
{
    if (m_stack->currentIndex() == 1) solve();
}

void InequalityPage::solve()
{
    m_error->hide();
    try {
        std::vector<double> c = readGrid()[0];
        if (c[0] == 0) throw CalcError(ErrKind::Math);
        auto roots = num::polyRoots(c);
        std::vector<double> rs;
        for (const auto &r : roots)
            if (r.imag() == 0) rs.push_back(r.real());
        std::sort(rs.begin(), rs.end());
        rs.erase(std::unique(rs.begin(), rs.end(), [](double a, double b) {
                     return std::abs(a - b) <= 1e-12 * std::max(1.0, std::abs(a));
                 }), rs.end());

        auto P = [&](double x) {
            double f = 0;
            for (double k : c) f = f * x + k;
            return f;
        };
        int rel = m_rel->currentIndex();   // 0 >, 1 <, 2 ≥, 3 ≤
        bool strict = rel < 2;
        auto sat = [&](double v) { return (rel == 0 || rel == 2) ? v > 0 : v < 0; };
        const size_t k = rs.size();
        std::vector<bool> ok(k + 1);
        for (size_t j = 0; j <= k; ++j) {
            double x;
            if (k == 0) x = 0;
            else if (j == 0) x = rs[0] - 1;
            else if (j == k) x = rs[k - 1] + 1;
            else x = 0.5 * (rs[j - 1] + rs[j]);
            ok[j] = sat(P(x));
        }

        DisplayOpts o;
        o.approx = m_approx;
        auto fmt = [&](double x) {
            QString s = formatReal(x, settings(), o);
            return s.startsWith(QStringLiteral("−")) || s.contains('/') ? "{" + s + "}" : s;
        };
        QStringList pieces;
        const QString lt = strict ? "<" : "≤";
        if (k == 0) {
            if (ok[0]) pieces << "All Real Numbers";
        } else if (strict) {
            for (size_t j = 0; j <= k; ++j) {
                if (!ok[j]) continue;
                if (j == 0) pieces << "x" + lt + fmt(rs[0]);
                else if (j == k) pieces << fmt(rs[k - 1]) + lt + "x";
                else pieces << fmt(rs[j - 1]) + lt + "x" + lt + fmt(rs[j]);
            }
        } else {
            size_t j = 0;
            std::vector<bool> usedRoot(k, false);
            while (j <= k) {
                if (!ok[j]) { ++j; continue; }
                size_t start = j;
                while (j < k && ok[j + 1]) ++j;
                bool lowInf = start == 0, highInf = j == k;
                if (!lowInf) usedRoot[start - 1] = true;
                if (!highInf) usedRoot[j] = true;
                for (size_t q = start; q < j; ++q) usedRoot[q] = true;
                if (lowInf && highInf) pieces << "All Real Numbers";
                else if (lowInf) pieces << "x" + lt + fmt(rs[j]);
                else if (highInf) pieces << fmt(rs[start - 1]) + lt + "x";
                else pieces << fmt(rs[start - 1]) + lt + "x" + lt + fmt(rs[j]);
                ++j;
            }
            // isolated roots (touching points) also satisfy ≥ / ≤
            QStringList points;
            for (size_t i = 0; i < k; ++i)
                if (!usedRoot[i]) points << "x=" + fmt(rs[i]);
            pieces = points + pieces;
        }
        MathStyle ms = mathStyle(20);
        std::vector<BoxPtr> lines;
        if (pieces.isEmpty()) lines.push_back(layoutText("No Solution", ms));
        for (const QString &p : pieces)
            lines.push_back(p == "All Real Numbers" ? layoutText(p, ms) : layoutSyntax(p, ms));
        QStringList names = {"a", "b", "c", "d", "e"};
        QString poly;
        int deg = int(c.size()) - 1;
        for (int i = 0; i <= deg; ++i) {
            poly += names[i];
            if (deg - i > 1) poly += "x" + QString(deg - i == 2 ? "²" : deg - i == 3 ? "³" : "⁴");
            else if (deg - i == 1) poly += "x";
            if (i < deg) poly += "+";
        }
        m_resTitle->setText(fontSafe(poly + " " + m_rel->currentText(), m_resTitle->font()));
        m_result->setBox(layoutLines(lines, false, 6));
        m_stack->setCurrentIndex(1);
    } catch (const CalcError &e) {
        m_error->setText(errorName(e.kind));
        m_error->show();
        m_stack->setCurrentIndex(0);
    }
}

void InequalityPage::execute(bool approx)
{
    if (m_stack->currentIndex() == 1) return;
    m_approx = approx;
    solve();
}

void InequalityPage::allClear()
{
    if (m_stack->currentIndex() == 1) {
        m_stack->setCurrentIndex(0);
        m_coef->setFocus();
        return;
    }
    ModePage::allClear();
}

QList<MenuItem> InequalityPage::optnMenu() const
{
    QList<MenuItem> deg, rel;
    for (int i = 0; i < m_degree->count(); ++i) deg.push_back({m_degree->itemText(i), QString("c:TYPE:%1").arg(i)});
    for (int i = 0; i < m_rel->count(); ++i) rel.push_back({m_rel->itemText(i), QString("c:REL:%1").arg(i)});
    return {{"Select Degree", {}, deg}, {"Select Relation", {}, rel}};
}

void InequalityPage::command(const QString &cmd)
{
    if (cmd.startsWith("TYPE:")) { m_degree->setCurrentIndex(cmd.mid(5).toInt()); m_stack->setCurrentIndex(0); return; }
    if (cmd.startsWith("REL:")) { m_rel->setCurrentIndex(cmd.mid(4).toInt()); return; }
    if (cmd == "SD" && m_stack->currentIndex() == 1) {
        m_approx = !m_approx;
        solve();
    }
}

// ------------------------------------------------------------------ RatioPage

RatioPage::RatioPage(AppState *st, QWidget *parent) : ModePage(st, parent)
{
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(10, 6, 10, 8);
    m_type = new QComboBox(this);
    m_type->addItems({"A : B = X : D", "A : B = C : X"});
    m_type->setFocusPolicy(Qt::NoFocus);
    v->addWidget(m_type);
    m_eq = new MathView(this);
    v->addWidget(m_eq);
    auto *form = new QFormLayout;
    m_a = new ExprEdit(this);
    m_b = new ExprEdit(this);
    m_c = new ExprEdit(this);
    m_a->setText("1");
    m_b->setText("2");
    m_c->setText("10");
    form->addRow(lcdLabel("A :", this), m_a);
    form->addRow(lcdLabel("B :", this), m_b);
    m_cLabel = lcdLabel("D :", this);
    form->addRow(m_cLabel, m_c);
    v->addLayout(form);
    m_error = new QLabel(this);
    m_error->setObjectName("errorBox");
    m_error->hide();
    v->addWidget(m_error);
    m_result = new MathView(this);
    m_result->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_result->style().size = 26;
    v->addWidget(m_result);
    v->addStretch();
    auto *hint = new QLabel("[=] : Solve for X      [S⇔D] : Exact / Decimal", this);
    hint->setObjectName("hint");
    v->addWidget(hint);
    for (auto *e : {m_a, m_b, m_c})
        connect(e, &QLineEdit::returnPressed, this, [this, e] {
            if (e == m_a) m_b->setFocus();
            else if (e == m_b) m_c->setFocus();
            else execute(false);
        });
    connect(m_type, &QComboBox::currentIndexChanged, this, [this] { updateEquation(); });
    updateEquation();
}

QWidget *RatioPage::editor() const { return m_a; }

void RatioPage::activated() { m_a->setFocus(); }

void RatioPage::updateEquation()
{
    bool second = m_type->currentIndex() == 1;
    m_cLabel->setText(second ? "C :" : "D :");
    m_eq->setBox(layoutText(second ? "A : B = C : X" : "A : B = X : D", mathStyle(20)));
    m_result->clear();
}

void RatioPage::execute(bool approx)
{
    m_approx = approx;
    m_error->hide();
    try {
        auto val = [&](ExprEdit *e) {
            Value v = evalString(e->text().trimmed().isEmpty() ? "0" : e->text());
            if (!v.isReal()) throw CalcError(ErrKind::Math);
            return v.re();
        };
        double a = val(m_a), b = val(m_b), c = val(m_c);
        double x;
        if (m_type->currentIndex() == 0) {
            if (b == 0) throw CalcError(ErrKind::Math);
            x = a * c / b;   // A:B = X:D  →  X = A·D / B
        } else {
            if (a == 0) throw CalcError(ErrKind::Math);
            x = b * c / a;   // A:B = C:X  →  X = B·C / A
        }
        ctx().pushAns(Value(x));
        DisplayOpts o;
        o.approx = approx;
        MathStyle ms = mathStyle(26);
        m_result->setBox(layoutLabeled("X", layoutValue(Value(x), settings(), o, ms), ms));
    } catch (const CalcError &e) {
        m_result->clear();
        m_error->setText(errorName(e.kind));
        m_error->show();
    }
}

void RatioPage::navigate(const QString &dir)
{
    if (dir == "UP") focusPreviousChild();
    else if (dir == "DOWN") focusNextChild();
    else ModePage::navigate(dir);
}

void RatioPage::command(const QString &cmd)
{
    if (cmd.startsWith("TYPE:")) { m_type->setCurrentIndex(cmd.mid(5).toInt()); return; }
    if (cmd == "SD" && m_result->box()) execute(!m_approx);
}
