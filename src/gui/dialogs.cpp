#include "dialogs.h"
#include "richtext.h"
#include "theme.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDialogButtonBox>
#include <QFormLayout>
#include <QHeaderView>
#include <QLabel>
#include <QMouseEvent>
#include <QPainter>
#include <QPainterPath>
#include <QPushButton>
#include <QSpinBox>
#include <QTableWidget>
#include <QVBoxLayout>

// ------------------------------------------------------------------ MenuPage

namespace {
struct ModeIcon {
    QString glyph;
    QColor color;
};
const ModeIcon icons[12] = {
    {"×÷", QColor(60, 110, 170)},   {"i", QColor(130, 80, 160)},        {"0b", QColor(50, 130, 110)},
    {"[ ]", QColor(170, 90, 60)},             {"→", QColor(70, 120, 60)},    {"x̄", QColor(180, 120, 30)},
    {"μσ", QColor(150, 60, 90)},    {"A1", QColor(40, 120, 150)},       {"f(x)", QColor(90, 90, 160)},
    {"=?", QColor(160, 70, 70)},              {"≤", QColor(100, 120, 50)},   {"a:b", QColor(120, 90, 70)},
};
} // namespace

MenuPage::MenuPage(QWidget *parent) : QWidget(parent)
{
    setMouseTracking(true);
    setFocusPolicy(Qt::StrongFocus);
}

QStringList MenuPage::modeNames()
{
    return {"Calculate", "Complex", "Base-N", "Matrix", "Vector", "Statistics", "Distribution", "Spreadsheet",
            "Table", "Equation/Func", "Inequality", "Ratio"};
}

QRectF MenuPage::tileRect(int i) const
{
    const int cols = 4, rows = 3;
    qreal m = 8, top = 6;
    qreal w = (width() - m * (cols + 1)) / cols;
    qreal h = (height() - top - m * (rows + 1)) / rows;
    int r = i / cols, c = i % cols;
    return QRectF(m + c * (w + m), top + m + r * (h + m), w, h);
}

int MenuPage::tileAt(const QPointF &p) const
{
    for (int i = 0; i < 12; ++i)
        if (tileRect(i).contains(p)) return i;
    return -1;
}

void MenuPage::setSelected(int i)
{
    m_sel = std::clamp(i, 0, 11);
    update();
}

void MenuPage::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    const QStringList names = modeNames();
    const QString keys = "123456789ABC";
    for (int i = 0; i < 12; ++i) {
        QRectF r = tileRect(i);
        bool sel = i == m_sel, hov = i == m_hover;
        QPainterPath path;
        path.addRoundedRect(r, 6, 6);
        QColor panel = themeColor("lcdPanel");
        if (hov && !sel) panel = panel.lightness() > 128 ? panel.darker(106) : panel.lighter(130);
        p.fillPath(path, sel ? themeColor("lcdAccent") : panel);
        p.setPen(QPen(themeColor("lcdLine"), 1));
        p.drawPath(path);
        // icon
        QRectF ir(r.center().x() - r.height() * 0.22, r.top() + r.height() * 0.12, r.height() * 0.44, r.height() * 0.40);
        QPainterPath ip;
        ip.addRoundedRect(ir, 5, 5);
        p.fillPath(ip, icons[i].color);
        QFont f = font();
        f.setPixelSize(int(ir.height() * 0.45));
        f.setBold(true);
        p.setFont(f);
        p.setPen(Qt::white);
        p.drawText(ir, Qt::AlignCenter, icons[i].glyph);
        // key + name
        f.setPixelSize(12);
        f.setBold(false);
        p.setFont(f);
        p.setPen(sel ? themeColor("lcdAccentText") : themeColor("lcdText"));
        QRectF tr(r.left() + 2, ir.bottom() + 2, r.width() - 4, r.bottom() - ir.bottom() - 4);
        p.drawText(tr, Qt::AlignHCenter | Qt::AlignTop | Qt::TextWordWrap, QString("%1: %2").arg(keys[i]).arg(names[i]));
    }
}

void MenuPage::mousePressEvent(QMouseEvent *e)
{
    int i = tileAt(e->position());
    if (i >= 0) {
        m_sel = i;
        update();
        emit modeSelected(i);
    }
}

void MenuPage::mouseMoveEvent(QMouseEvent *e)
{
    int i = tileAt(e->position());
    if (i != m_hover) {
        m_hover = i;
        update();
    }
}

void MenuPage::navigate(const QString &dir)
{
    int s = m_sel;
    if (dir == "LEFT") s = (s + 11) % 12;
    if (dir == "RIGHT") s = (s + 1) % 12;
    if (dir == "UP") s = (s + 8) % 12;
    if (dir == "DOWN") s = (s + 4) % 12;
    setSelected(s);
}

bool MenuPage::handleKey(const QString &id)
{
    static const QMap<QString, int> map = {{"1", 0}, {"2", 1}, {"3", 2}, {"4", 3}, {"5", 4}, {"6", 5},
                                           {"7", 6}, {"8", 7}, {"9", 8}, {"neg", 9}, {"dms", 10}, {"inv", 11}};
    if (map.contains(id)) {
        setSelected(map[id]);
        emit modeSelected(map[id]);
        return true;
    }
    if (id == "exe") {
        emit modeSelected(m_sel);
        return true;
    }
    return false;
}

// ------------------------------------------------------------------ SetupDialog

SetupDialog::SetupDialog(Settings &s, QWidget *parent) : QDialog(parent), m_s(s)
{
    setWindowTitle("SETUP");
    auto *v = new QVBoxLayout(this);
    auto *f = new QFormLayout;
    m_io = new QComboBox(this);
    m_io->addItems({"MathI / MathO", "MathI / DecimalO", "LineI / LineO", "LineI / DecimalO"});
    m_io->setCurrentIndex(int(s.io));
    m_angle = new QComboBox(this);
    m_angle->addItems({"Degree (D)", "Radian (R)", "Gradian (G)"});
    m_angle->setCurrentIndex(int(s.angle));
    m_format = new QComboBox(this);
    m_format->addItems({"Norm 1", "Norm 2", "Fix", "Sci"});
    m_format->setCurrentIndex(int(s.format));
    m_digits = new QSpinBox(this);
    m_digits->setRange(0, 9);
    m_digits->setValue(s.digits);
    m_digits->setToolTip("Fix: decimal places (0-9).  Sci: significant digits (0 = 10).");
    m_frac = new QComboBox(this);
    m_frac->addItems({"d/c  (improper)", "a⌟b⌟c  (mixed)"});
    m_frac->setCurrentIndex(int(s.frac));
    m_complex = new QComboBox(this);
    m_complex->addItems({"a+bi", "r∠θ"});
    m_complex->setCurrentIndex(int(s.complexForm));
    m_freq = new QCheckBox("Frequency column on", this);
    m_freq->setChecked(s.statFrequency);
    m_auto = new QCheckBox("Auto calc", this);
    m_auto->setChecked(s.sheetAutoCalc);
    m_tableG = new QCheckBox("f(x), g(x)", this);
    m_tableG->setChecked(s.tableUseG);
    m_decimal = new QComboBox(this);
    m_decimal->addItems({"Dot  ( . )", "Comma  ( , )"});
    m_decimal->setCurrentIndex(s.decimalComma ? 1 : 0);
    m_sep = new QCheckBox("Show digit separators", this);
    m_sep->setChecked(s.digitSeparator);
    f->addRow("Input/Output", m_io);
    f->addRow("Angle Unit", m_angle);
    f->addRow("Number Format", m_format);
    f->addRow("  Digits (Fix / Sci)", m_digits);
    f->addRow("Fraction Result", m_frac);
    f->addRow("Complex", m_complex);
    f->addRow("Statistics", m_freq);
    f->addRow("Spreadsheet", m_auto);
    f->addRow("Table", m_tableG);
    f->addRow("Decimal Mark", m_decimal);
    f->addRow("Digit Separator", m_sep);
    v->addLayout(f);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel | QDialogButtonBox::RestoreDefaults, this);
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(bb->button(QDialogButtonBox::RestoreDefaults), &QPushButton::clicked, this, [this] {
        Settings d;
        m_io->setCurrentIndex(int(d.io));
        m_angle->setCurrentIndex(int(d.angle));
        m_format->setCurrentIndex(int(d.format));
        m_digits->setValue(d.digits);
        m_frac->setCurrentIndex(int(d.frac));
        m_complex->setCurrentIndex(int(d.complexForm));
        m_freq->setChecked(d.statFrequency);
        m_auto->setChecked(d.sheetAutoCalc);
        m_tableG->setChecked(d.tableUseG);
        m_decimal->setCurrentIndex(0);
        m_sep->setChecked(false);
    });
    v->addWidget(bb);
    makeFontSafe(this);
}

void SetupDialog::accept()
{
    m_s.io = IOMode(m_io->currentIndex());
    m_s.angle = AngleUnit(m_angle->currentIndex());
    m_s.format = NumFormat(m_format->currentIndex());
    m_s.digits = m_digits->value();
    m_s.frac = FracFormat(m_frac->currentIndex());
    m_s.complexForm = ComplexForm(m_complex->currentIndex());
    m_s.statFrequency = m_freq->isChecked();
    m_s.sheetAutoCalc = m_auto->isChecked();
    m_s.tableUseG = m_tableG->isChecked();
    m_s.decimalComma = m_decimal->currentIndex() == 1;
    m_s.digitSeparator = m_sep->isChecked();
    QDialog::accept();
}

// ------------------------------------------------------------------ RecallDialog

RecallDialog::RecallDialog(const Context &ctx, QWidget *parent) : QDialog(parent)
{
    setWindowTitle("RCL  —  variables");
    resize(380, 420);
    auto *v = new QVBoxLayout(this);
    auto *t = new QTableWidget(0, 2, this);
    t->setItemDelegate(new RichDelegate(t));
    t->setHorizontalHeaderLabels({"Variable", "Value"});
    t->horizontalHeader()->setSectionResizeMode(0, QHeaderView::ResizeToContents);
    t->horizontalHeader()->setSectionResizeMode(1, QHeaderView::Stretch);
    t->verticalHeader()->hide();
    t->setEditTriggers(QAbstractItemView::NoEditTriggers);
    t->setSelectionBehavior(QAbstractItemView::SelectRows);
    Settings s = ctx.settings;
    const QStringList names = {"A", "B", "C", "D", "E", "F", "M", "x", "y", "Ans", "PreAns",
                               "MatA", "MatB", "MatC", "MatD", "MatAns", "VctA", "VctB", "VctC", "VctD", "VctAns"};
    for (const QString &n : names) {
        if (!ctx.vars.contains(n)) continue;
        const Value &val = ctx.vars[n];
        QString text;
        if (val.isScalar()) {
            text = toLinearText(formatComplex(val.z, s));
            DisplayOpts a;
            a.approx = true;
            QString dec = toLinearText(formatComplex(val.z, s, a));
            if (dec != text) text += "   (" + dec + ")";
        } else {
            QStringList rows;
            for (int r = 0; r < val.rows; ++r) {
                QStringList cols;
                for (int c = 0; c < val.cols; ++c) cols << toLinearText(formatComplex(val.at(r, c), s));
                rows << "[" + cols.join(", ") + "]";
            }
            text = (val.isMatrix() ? QString("%1×%2  ").arg(val.rows).arg(val.cols) : QString("dim %1  ").arg(val.rows)) +
                   rows.join(" ");
        }
        int r = t->rowCount();
        t->insertRow(r);
        t->setItem(r, 0, new QTableWidgetItem(n));
        t->setItem(r, 1, new QTableWidgetItem(text));
    }
    v->addWidget(new QLabel("Double-click a variable to insert it into the expression."));
    v->addWidget(t);
    auto *bb = new QDialogButtonBox(QDialogButtonBox::Close, this);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    v->addWidget(bb);
    connect(t, &QTableWidget::cellDoubleClicked, this, [this, t](int r, int) {
        m_chosen = t->item(r, 0)->text();
        accept();
    });
}
