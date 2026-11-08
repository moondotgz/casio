#include "statpage.h"
#include "theme.h"
#include <QApplication>
#include <QCheckBox>
#include <QComboBox>
#include <QHBoxLayout>
#include <QHeaderView>
#include <QLabel>
#include <QTabWidget>
#include <QTableWidget>
#include <cmath>

namespace {
const QString XHAT = QStringLiteral("x̂");
const QString YHAT = QStringLiteral("ŷ");
const QString XHAT1 = QStringLiteral("x̂₁");
const QString XHAT2 = QStringLiteral("x̂₂");
}

StatPage::StatPage(AppState *st, QWidget *parent) : ModePage(st, parent)
{
    auto *v = new QVBoxLayout(this);
    v->setContentsMargins(6, 4, 6, 6);
    auto *top = new QHBoxLayout;
    m_type = new QComboBox(this);
    for (RegType t : {RegType::OneVar, RegType::Linear, RegType::Quadratic, RegType::Log, RegType::ExpE, RegType::ExpAB,
                      RegType::Power, RegType::Inverse})
        m_type->addItem(regTypeName(t), int(t));
    m_type->setFocusPolicy(Qt::NoFocus);
    m_freq = new QCheckBox("Frequency", this);
    m_freq->setFocusPolicy(Qt::NoFocus);
    top->addWidget(m_type, 1);
    top->addWidget(m_freq);
    v->addLayout(top);

    m_tabs = new QTabWidget(this);
    m_tabs->setFocusPolicy(Qt::NoFocus);
    v->addWidget(m_tabs, 1);

    // data editor
    m_data = new QTableWidget(160, 1, this);
    m_data->setItemDelegate(new ExprDelegate(m_data));
    m_data->horizontalHeader()->setSectionResizeMode(QHeaderView::Stretch);
    m_data->verticalHeader()->setDefaultSectionSize(22);
    m_data->setEditTriggers(QAbstractItemView::AllEditTriggers);
    m_tabs->addTab(m_data, "Data");

    // results
    auto *resHost = new QWidget;
    auto *rv = new QVBoxLayout(resHost);
    rv->setContentsMargins(2, 2, 2, 2);
    m_resultTitle = lcdLabel("", resHost, true);
    rv->addWidget(m_resultTitle);
    m_results = new QTableWidget(0, 2, resHost);
    m_results->setItemDelegate(new RichDelegate(m_results));
    m_results->horizontalHeader()->hide();
    m_results->verticalHeader()->hide();
    m_results->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    m_results->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    m_results->setEditTriggers(QAbstractItemView::NoEditTriggers);
    m_results->verticalHeader()->setDefaultSectionSize(22);
    rv->addWidget(m_results);
    m_tabs->addTab(resHost, "Results");

    // statistic calculation
    auto *calcHost = new QWidget;
    auto *cv = new QVBoxLayout(calcHost);
    cv->setContentsMargins(4, 4, 4, 4);
    auto *hint = new QLabel("Use statistic variables (OPTN) such as x̄, σx, a, b, r, and the estimates "
                            "x̂(y), ŷ(x).", calcHost);
    hint->setObjectName("hint");
    hint->setWordWrap(true);
    cv->addWidget(hint);
    cv->addStretch();
    m_calcResult = new MathView(calcHost);
    m_calcResult->setAlignment(Qt::AlignRight | Qt::AlignVCenter);
    m_calcResult->style().size = 24;
    cv->addWidget(m_calcResult);
    m_calcError = new QLabel(calcHost);
    m_calcError->setObjectName("errorBox");
    m_calcError->hide();
    cv->addWidget(m_calcError);
    m_calc = new ExprEdit(calcHost);
    m_calc->setPlaceholderText("e.g.  ŷ(5)   or   x̄ + 2σx");
    cv->addWidget(m_calc);
    m_tabs->addTab(calcHost, "Calc");

    connect(m_type, &QComboBox::currentIndexChanged, this, [this] { setupColumns(); m_stat.reset(); });
    connect(m_freq, &QCheckBox::toggled, this, [this](bool on) {
        this->st->settings().statFrequency = on;
        setupColumns();
        m_stat.reset();
    });
    connect(m_data, &QTableWidget::cellChanged, this, &StatPage::cellChanged);
    connect(m_calc, &QLineEdit::returnPressed, this, [this] { execute(false); });
    connect(m_tabs, &QTabWidget::currentChanged, this, [this](int i) {
        if (i == 2) m_calc->setFocus();
        if (i == 1 && !m_stat) compute();
    });
    m_freq->setChecked(st->settings().statFrequency);
    setupColumns();
}

RegType StatPage::type() const { return RegType(m_type->currentData().toInt()); }

QWidget *StatPage::editor() const { return m_tabs->currentIndex() == 2 ? m_calc : nullptr; }

void StatPage::activated()
{
    m_freq->setChecked(settings().statFrequency);
    if (m_tabs->currentIndex() == 0) m_data->setFocus();
}

void StatPage::settingsChanged()
{
    m_freq->setChecked(settings().statFrequency);
    if (m_stat) showResults(m_tabs->currentIndex() == 1 && regIsPaired(type()));
}

void StatPage::setupColumns()
{
    QSignalBlocker b(m_data);
    QStringList h = {"x"};
    if (regIsPaired(type())) h << "y";
    if (m_freq->isChecked()) h << "Freq";
    int oldCols = m_data->columnCount();
    m_data->setColumnCount(int(h.size()));
    m_data->setHorizontalHeaderLabels(h);
    // a new frequency column defaults to 1 for existing rows
    if (m_freq->isChecked()) {
        int fc = int(h.size()) - 1;
        for (int r = 0; r < m_data->rowCount(); ++r) {
            bool hasX = m_data->item(r, 0) && !m_data->item(r, 0)->text().isEmpty();
            auto *it = m_data->item(r, fc);
            if (hasX && (!it || it->text().isEmpty() || oldCols <= fc)) m_data->setItem(r, fc, new QTableWidgetItem("1"));
        }
    }
}

void StatPage::cellChanged(int row, int col)
{
    QTableWidgetItem *it = m_data->item(row, col);
    if (!it) return;
    QString t = it->text().trimmed();
    QSignalBlocker b(m_data);
    if (t.isEmpty()) return;
    try {
        Value v = evalString(t);
        if (!v.isReal()) throw CalcError(ErrKind::Math);
        Settings s = settings();
        s.format = NumFormat::Norm1;
        it->setText(toLinearText(formatDecimal(v.re(), s)));
        it->setData(Qt::UserRole, v.re());
        it->setData(Qt::ForegroundRole, QVariant());
        // default frequency
        if (m_freq->isChecked() && col == 0) {
            int fc = m_data->columnCount() - 1;
            if (!m_data->item(row, fc) || m_data->item(row, fc)->text().isEmpty())
                m_data->setItem(row, fc, new QTableWidgetItem("1"));
        }
    } catch (const CalcError &e) {
        it->setForeground(themeColor("lcdError"));
        emit message(errorName(e.kind) + QString(" in row %1").arg(row + 1));
    }
    m_stat.reset();
}

StatData StatPage::collect()
{
    flushTableEditor(m_data);
    StatData d;
    bool paired = regIsPaired(type());
    bool freq = m_freq->isChecked();
    auto cell = [&](int r, int c, double &out) {
        auto *it = m_data->item(r, c);
        if (!it || it->text().trimmed().isEmpty()) return false;
        Value v = evalString(it->text().trimmed());
        if (!v.isReal()) throw CalcError(ErrKind::Math);
        out = v.re();
        return true;
    };
    for (int r = 0; r < m_data->rowCount(); ++r) {
        double x, y = 0, f = 1;
        bool hx = cell(r, 0, x);
        if (!hx) continue;
        if (paired && !cell(r, 1, y)) y = 0;
        if (freq) cell(r, m_data->columnCount() - 1, f);
        d.x.push_back(x);
        if (paired) d.y.push_back(y);
        d.f.push_back(f);
    }
    return d;
}

bool StatPage::compute()
{
    try {
        StatData d = collect();
        m_stat = computeStats(d, type());
        showResults(regIsPaired(type()));
        return true;
    } catch (const CalcError &e) {
        m_stat.reset();
        m_results->setRowCount(0);
        m_resultTitle->setText(e.kind == ErrKind::Math ? "No data / " + errorName(e.kind) : errorName(e.kind));
        return false;
    }
}

void StatPage::showResults(bool regression)
{
    if (!m_stat) return;
    const StatResult &r = *m_stat;
    QStringList order = r.oneVarOrder;
    if (regIsPaired(r.type)) order += r.pairedOrder;
    if (regression) order = r.regOrder + QStringList{"---"} + order;
    m_resultTitle->setText(regIsPaired(r.type) ? "Regression  " + regTypeName(r.type) : "1-Variable statistics");
    m_results->setRowCount(0);
    for (const QString &k : order) {
        int row = m_results->rowCount();
        m_results->insertRow(row);
        if (k == "---") {
            m_results->setItem(row, 0, new QTableWidgetItem(""));
            continue;
        }
        auto v = r.get(k);
        QString label = k == "minX" ? "min(x)" : k == "maxX" ? "max(x)" : k == "minY" ? "min(y)" : k == "maxY" ? "max(y)" : k;
        m_results->setItem(row, 0, new QTableWidgetItem(label + " ="));
        m_results->setItem(row, 1, new QTableWidgetItem(v ? toDisplayText(formatDecimal(*v, settings())) : QString("—")));
        m_results->item(row, 1)->setTextAlignment(Qt::AlignRight | Qt::AlignVCenter);
    }
}

void StatPage::prepareContext()
{
    ModePage::prepareContext();
    if (!m_stat) return;
    const StatResult r = *m_stat;
    ctx().varHook = [r](const QString &name) -> std::optional<Value> {
        if (auto v = r.get(name)) return Value(*v);
        return std::nullopt;
    };
    ctx().funcHook = [r](const QString &name, const std::vector<Value> &args) -> std::optional<Value> {
        if (args.size() != 1 || !args[0].isReal()) return std::nullopt;
        double a = args[0].re();
        if (!regIsPaired(r.type)) throw CalcError(ErrKind::Syntax);
        if (name == YHAT) return Value(r.estimateY(a));
        if (name == XHAT || name == XHAT1) return Value(r.estimateX(a)[0]);
        if (name == XHAT2) {
            auto xs = r.estimateX(a);
            if (xs.size() < 2) throw CalcError(ErrKind::Syntax);
            return Value(xs[1]);
        }
        return std::nullopt;
    };
}

void StatPage::execute(bool approx)
{
    int tab = m_tabs->currentIndex();
    if (tab == 0) {
        if (compute()) m_tabs->setCurrentIndex(1);
        return;
    }
    if (tab == 1) {
        compute();
        return;
    }
    // statistic calculation line
    m_calcError->hide();
    if (!m_stat && !compute()) {
        m_calcError->setText("No data");
        m_calcError->show();
        return;
    }
    QString text = m_calc->text().trimmed();
    if (text.isEmpty()) return;
    try {
        prepareContext();
        ParseOptions po;
        po.lex.extraVars = statVariableNames();
        po.lex.extraFuncs = {XHAT1, XHAT2, XHAT, YHAT};
        Evaluator ev(ctx());
        Value v = ev.evaluate(parseExpression(text, po));
        ctx().pushAns(v);
        DisplayOpts o;
        o.approx = approx;
        m_calcResult->setBox(layoutValue(v, settings(), o, mathStyle(24)));
    } catch (const CalcError &e) {
        m_calcResult->clear();
        m_calcError->setText(errorName(e.kind));
        m_calcError->show();
        if (e.pos >= 0) m_calc->setCursorPosition(e.pos);
    }
}

void StatPage::allClear()
{
    if (m_tabs->currentIndex() == 2) {
        m_calc->clear();
        m_calcResult->clear();
        m_calcError->hide();
        return;
    }
    if (m_tabs->currentIndex() == 1) {
        m_tabs->setCurrentIndex(0);
        return;
    }
    ModePage::allClear();
}

void StatPage::command(const QString &cmd)
{
    if (cmd.startsWith("TYPE:")) { m_type->setCurrentIndex(cmd.mid(5).toInt()); m_tabs->setCurrentIndex(0); }
    else if (cmd == "FREQ") m_freq->toggle();
    else if (cmd == "STAT_DATA") m_tabs->setCurrentIndex(0);
    else if (cmd == "STAT_1VAR") { if (compute()) { showResults(false); m_tabs->setCurrentIndex(1); } }
    else if (cmd == "STAT_REG") { if (compute()) { showResults(true); m_tabs->setCurrentIndex(1); } }
    else if (cmd == "STAT_CALC") m_tabs->setCurrentIndex(2);
    else if (cmd == "STAT_CLEAR") {
        QSignalBlocker b(m_data);
        m_data->clearContents();
        m_stat.reset();
        m_results->setRowCount(0);
    } else if (cmd == "STAT_DELROW") {
        int r = m_data->currentRow();
        if (r >= 0) { m_data->removeRow(r); m_data->insertRow(m_data->rowCount()); m_stat.reset(); }
    } else if (cmd == "STAT_INSROW") {
        int r = m_data->currentRow();
        if (r >= 0) { m_data->insertRow(r); m_data->removeRow(m_data->rowCount() - 1); m_stat.reset(); }
    }
}

QList<MenuItem> StatPage::optnMenu() const
{
    auto var = [](const QString &n) { return MenuItem(n, "i:" + n); };
    QList<MenuItem> m = {
        {"Select Type", {}, {}},
        {"Data", {}, {{"Data editor", "c:STAT_DATA"}, {"Frequency on/off", "c:FREQ"}, {"Insert row", "c:STAT_INSROW"},
                                     {"Delete row", "c:STAT_DELROW"}, {"Delete all data", "c:STAT_CLEAR"}}},
        {"1-Variable Calc", "c:STAT_1VAR"},
        {"Regression Calc", "c:STAT_REG"},
        {"Statistic Calculation line", "c:STAT_CALC"},
        {"Summation", {}, {var("Σx"), var("Σx²"), var("Σy"), var("Σy²"), var("Σxy"),
                           var("Σx³"), var("Σx²y"), var("Σx⁴")}},
        {"Variable", {}, {var("n"), var("x̄"), var("σx"), var("sx"), var("ȳ"), var("σy"), var("sy")}},
        {"Min / Max / Quartile", {}, {var("minX"), var("maxX"), var("minY"), var("maxY"), var("Q1"), var("Med"), var("Q3")}},
        {"Regression", {}, {var("a"), var("b"), var("c"), var("r"), {XHAT + "(", "i:" + XHAT + "(§)"},
                            {XHAT1 + "(", "i:" + XHAT1 + "(§)"}, {XHAT2 + "(", "i:" + XHAT2 + "(§)"},
                            {YHAT + "(", "i:" + YHAT + "(§)"}}},
    };
    for (int i = 0; i < m_type->count(); ++i) m[0].children.push_back({m_type->itemText(i), QString("c:TYPE:%1").arg(i)});
    return m;
}

void StatPage::insertText(const QString &text, bool startsNew)
{
    int tab = m_tabs->currentIndex();
    if (tab == 1) {
        m_tabs->setCurrentIndex(2);
        tab = 2;
    }
    if (tab == 0) beginCellInput(m_data);
    else m_calc->setFocus();
    ModePage::insertText(text, startsNew);
}

void StatPage::navigate(const QString &dir)
{
    if (m_tabs->currentIndex() != 0) { ModePage::navigate(dir); return; }
    navigateTable(m_data, dir);
}
