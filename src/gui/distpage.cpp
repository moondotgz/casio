#include "distpage.h"
#include "core/numeric.h"
#include <QApplication>
#include <QComboBox>
#include <QFormLayout>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QRadioButton>
#include <QTableWidget>
#include <cmath>

DistPage::DistPage(AppState *st, QWidget *parent) : ModePage(st, parent)
{
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(8, 4, 8, 6);
    m_type = new QComboBox(this);
    m_type->addItems({"Normal PD", "Normal CD", "Inverse Normal", "Binomial PD", "Binomial CD", "Poisson PD", "Poisson CD"});
    m_type->setFocusPolicy(Qt::NoFocus);
    v->addWidget(m_type);

    auto *mode = new QHBoxLayout;
    m_variable = new QRadioButton("Variable", this);
    m_list = new QRadioButton("List", this);
    m_variable->setChecked(true);
    for (auto *b : {m_variable, m_list}) b->setFocusPolicy(Qt::NoFocus);
    mode->addWidget(m_variable);
    mode->addWidget(m_list);
    mode->addStretch();
    v->addLayout(mode);

    auto *formHost = new QWidget(this);
    m_form = new QFormLayout(formHost);
    m_form->setContentsMargins(0, 2, 0, 2);
    v->addWidget(formHost);
    m_tailLabel = lcdLabel("Tail", this);
    m_tail = new QComboBox(this);
    m_tail->addItems({"Left", "Right", "Center"});
    m_tail->setFocusPolicy(Qt::NoFocus);
    m_tailLabel->hide();
    m_tail->hide();

    m_error = new QLabel(this);
    m_error->setObjectName("errorBox");
    m_error->hide();
    v->addWidget(m_error);
    m_result = new MathView(this);
    m_result->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    v->addWidget(m_result);
    m_table = new QTableWidget(0, 2, this);
    m_table->setItemDelegate(new RichDelegate(m_table));
    m_table->setHorizontalHeaderLabels({"x", "P"});
    m_table->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_table->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_table->verticalHeader()->setDefaultSectionSize(22);
    m_table->hide();
    v->addWidget(m_table, 1);
    v->addStretch();
    auto *hint = new QLabel("[=] : Calculate      [AC] : Clear result      ±1ᴇ99 = ±∞ bounds", this);
    hint->setObjectName("hint");
    v->addWidget(hint);

    connect(m_type, &QComboBox::currentIndexChanged, this, [this] { rebuildForm(); });
    connect(m_list, &QRadioButton::toggled, this, [this] { rebuildForm(); });
    rebuildForm();
}

DistPage::Type DistPage::type() const { return Type(m_type->currentIndex()); }

QWidget *DistPage::editor() const { return m_fields.isEmpty() ? nullptr : m_fields.first().second; }

void DistPage::activated()
{
    if (!m_fields.isEmpty()) m_fields.first().second->setFocus();
}

void DistPage::rebuildForm()
{
    QMap<QString, QString> keep;
    for (const auto &f : m_fields) keep[f.first] = f.second->text();
    m_form->removeWidget(m_tail);
    m_form->removeWidget(m_tailLabel);
    while (m_form->rowCount() > 0) m_form->removeRow(0);
    m_fields.clear();

    Type t = type();
    bool listOk = t != NormCD && t != InvNorm;
    m_variable->setEnabled(listOk);
    m_list->setEnabled(listOk);
    bool list = listOk && m_list->isChecked();

    QStringList names;
    switch (t) {
    case NormPD: names = {"x", "σ", "μ"}; break;
    case NormCD: names = {"Lower", "Upper", "σ", "μ"}; break;
    case InvNorm: names = {"Area", "σ", "μ"}; break;
    case BinPD: case BinCD: names = {"x", "N", "p"}; break;
    case PoiPD: case PoiCD: names = {"x", "λ"}; break;
    }
    if (t == InvNorm) {
        m_tailLabel->show();
        m_tail->show();
        m_form->addRow(m_tailLabel, m_tail);
    } else {
        m_tailLabel->hide();
        m_tail->hide();
    }
    static const QMap<QString, QString> defaults = {{"σ", "1"}, {"μ", "0"}, {"Lower", QStringLiteral("−") + "1"},
                                                    {"Upper", "1"}, {"Area", "0.5"}, {"N", "10"}, {"p", "0.5"},
                                                    {"λ", "1"}, {"x", "0"}};
    for (const QString &n : names) {
        auto *e = new ExprEdit;
        e->setText(keep.value(n, defaults.value(n)));
        if (n == "x" && list) {
            e->setPlaceholderText("list, e.g.  0,1,2,3");
            if (!e->text().contains(',')) e->setText("0,1,2,3,4");
        }
        connect(e, &QLineEdit::returnPressed, this, [this, e] {
            for (int i = 0; i < m_fields.size(); ++i)
                if (m_fields[i].second == e) {
                    if (i + 1 < m_fields.size()) m_fields[i + 1].second->setFocus();
                    else execute(false);
                    return;
                }
        });
        m_form->addRow(lcdLabel(n + (n == "x" && list ? " (List)" : "") + " :", nullptr), e);
        m_fields.push_back({n, e});
    }
    m_result->clear();
    m_table->hide();
    m_error->hide();
}

double DistPage::field(const QString &name)
{
    for (const auto &f : m_fields)
        if (f.first == name) {
            Value v = evalString(f.second->text().trimmed().isEmpty() ? "0" : f.second->text());
            if (!v.isReal()) throw CalcError(ErrKind::Math);
            return v.re();
        }
    throw CalcError(ErrKind::Argument);
}

void DistPage::execute(bool approx)
{
    Q_UNUSED(approx);
    m_error->hide();
    m_table->hide();
    m_result->clear();
    MathStyle ms = mathStyle(20);
    auto fmt = [&](double x) {
        Settings s = settings();
        return layoutSyntax(formatDecimal(x, s), ms);
    };
    try {
        Type t = type();
        bool list = m_list->isChecked() && m_list->isEnabled();
        std::vector<double> xs;
        if (t != NormCD && t != InvNorm) {
            QString xt;
            for (const auto &f : m_fields)
                if (f.first == "x") xt = f.second->text();
            if (list) {
                for (const QString &part : xt.split(',', Qt::SkipEmptyParts)) xs.push_back(evalString(part).re());
                if (xs.empty() || xs.size() > 45) throw CalcError(ErrKind::Argument);
            } else {
                xs.push_back(field("x"));
            }
        }
        auto P = [&](double x) -> double {
            switch (t) {
            case NormPD: return num::normalPdf(x, field("σ"), field("μ"));
            case BinPD: return num::binomialPdf(x, field("N"), field("p"));
            case BinCD: return num::binomialCdf(x, field("N"), field("p"));
            case PoiPD: return num::poissonPdf(x, field("λ"));
            case PoiCD: return num::poissonCdf(x, field("λ"));
            default: throw CalcError(ErrKind::Syntax);
            }
        };
        if (t == NormCD) {
            double lo = field("Lower"), up = field("Upper"), s = field("σ"), m = field("μ");
            double p = num::normalCdf(lo, up, s, m);
            ctx().pushAns(Value(p));
            m_result->setBox(layoutLines({layoutLabeled("P", fmt(p), ms), layoutLabeled("z:Low", fmt((lo - m) / s), ms),
                                          layoutLabeled("z:Up", fmt((up - m) / s), ms)}, true));
        } else if (t == InvNorm) {
            double a = field("Area"), s = field("σ"), m = field("μ");
            if (s <= 0 || a <= 0 || a >= 1) throw CalcError(ErrKind::Math);
            int tail = m_tail->currentIndex();
            if (tail == 0) {
                double x = m + s * num::invNormalStd(a);
                ctx().pushAns(Value(x));
                m_result->setBox(layoutLabeled("xInv", fmt(x), ms));
            } else if (tail == 1) {
                double x = m + s * num::invNormalStd(1 - a);
                ctx().pushAns(Value(x));
                m_result->setBox(layoutLabeled("xInv", fmt(x), ms));
            } else {
                double z = num::invNormalStd(0.5 + a / 2);
                ctx().pushAns(Value(m - s * z));
                m_result->setBox(layoutLines({layoutLabeled("x1Inv", fmt(m - s * z), ms),
                                              layoutLabeled("x2Inv", fmt(m + s * z), ms)}, true));
            }
        } else if (!list) {
            double p = P(xs[0]);
            ctx().pushAns(Value(p));
            m_result->setBox(layoutLabeled(t == NormPD ? "p" : "P", fmt(p), ms));
        } else {
            m_table->setRowCount(0);
            for (double x : xs) {
                int r = m_table->rowCount();
                m_table->insertRow(r);
                m_table->setItem(r, 0, new QTableWidgetItem(toDisplayText(formatDecimal(x, settings()))));
                QString pv;
                try {
                    pv = toDisplayText(formatDecimal(P(x), settings()));
                } catch (const CalcError &e) {
                    pv = errorName(e.kind);
                }
                m_table->setItem(r, 1, new QTableWidgetItem(pv));
            }
            m_table->setHorizontalHeaderLabels({"x", t == NormPD ? "p" : "P"});
            m_table->show();
        }
    } catch (const CalcError &e) {
        m_error->setText(errorName(e.kind));
        m_error->show();
    }
}

void DistPage::allClear()
{
    if (!m_result->box() && m_table->isHidden()) {
        ModePage::allClear();
        return;
    }
    m_result->clear();
    m_table->hide();
    m_error->hide();
}

void DistPage::navigate(const QString &dir)
{
    if (dir == "UP") focusPreviousChild();
    else if (dir == "DOWN") focusNextChild();
    else ModePage::navigate(dir);
}

QList<MenuItem> DistPage::optnMenu() const
{
    QList<MenuItem> types;
    for (int i = 0; i < m_type->count(); ++i) types.push_back({m_type->itemText(i), QString("c:TYPE:%1").arg(i)});
    return {{"Select Type", {}, types}, {"Variable", "c:VAR"}, {"List", "c:LIST"}};
}

void DistPage::command(const QString &cmd)
{
    if (cmd.startsWith("TYPE:")) m_type->setCurrentIndex(cmd.mid(5).toInt());
    else if (cmd == "VAR") m_variable->setChecked(true);
    else if (cmd == "LIST" && m_list->isEnabled()) m_list->setChecked(true);
}
