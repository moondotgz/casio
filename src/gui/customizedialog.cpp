#include "customizedialog.h"
#include <QCheckBox>
#include <QComboBox>
#include <QColorDialog>
#include <QDialogButtonBox>
#include <QFileDialog>
#include <QFontComboBox>
#include <QFormLayout>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QInputDialog>
#include <QJsonDocument>
#include <QLabel>
#include <QLineEdit>
#include <QListWidget>
#include <QMessageBox>
#include <QPainter>
#include <QPushButton>
#include <QScrollArea>
#include <QSlider>
#include <QTabWidget>
#include <QVBoxLayout>

namespace {
const QString kModified = QStringLiteral(" (modified)");

QSlider *makeSlider(int lo, int hi, QWidget *parent)
{
    auto *s = new QSlider(Qt::Horizontal, parent);
    s->setRange(lo, hi);
    return s;
}
} // namespace

CustomizeDialog::CustomizeDialog(QWidget *parent) : QDialog(parent)
{
    setWindowTitle("Customize appearance");
    resize(620, 560);
    auto &tm = ThemeManager::instance();
    m_origTheme = tm.theme();
    m_origApp = tm.appearance();

    auto *v = new QVBoxLayout(this);
    auto *tabs = new QTabWidget(this);
    tabs->addTab(buildThemeTab(), "Theme");
    tabs->addTab(buildDisplayTab(), "Display");
    tabs->addTab(buildKeypadTab(), "Keypad");
    tabs->addTab(buildWindowTab(), "Window");
    v->addWidget(tabs);

    auto *bb = new QDialogButtonBox(QDialogButtonBox::Ok | QDialogButtonBox::Cancel, this);
    auto *reset = bb->addButton("Reset all", QDialogButtonBox::ResetRole);
    connect(bb, &QDialogButtonBox::accepted, this, &QDialog::accept);
    connect(bb, &QDialogButtonBox::rejected, this, &QDialog::reject);
    connect(reset, &QPushButton::clicked, this, [this] {
        ThemeManager::instance().setTheme(ThemeManager::presets().first());
        ThemeManager::instance().setAppearance(Appearance{});
        loadControls();
    });
    v->addWidget(bb);

    loadControls();
    connect(&tm, &ThemeManager::changed, this, [this] {
        if (!m_loading) refreshSwatches();
    });
}

QIcon CustomizeDialog::themeIcon(const Theme &t)
{
    QPixmap pm(44, 28);
    pm.fill(Qt::transparent);
    QPainter p(&pm);
    p.setRenderHint(QPainter::Antialiasing);
    p.setPen(Qt::NoPen);
    p.setBrush(t.color("bodyBottom"));
    p.drawRoundedRect(QRectF(0, 0, 44, 28), 4, 4);
    p.setBrush(t.color("lcdBg"));
    p.drawRoundedRect(QRectF(4, 3, 36, 11), 2, 2);
    p.setBrush(t.color("lcdText"));
    p.drawRect(QRectF(7, 7, 14, 3));
    const char *keys[] = {"keyFunc", "keyNum", "keyAccent", "keyShift"};
    for (int i = 0; i < 4; ++i) {
        p.setBrush(t.color(keys[i]));
        p.drawRoundedRect(QRectF(4 + i * 9.3, 17, 7.5, 7), 1.5, 1.5);
    }
    p.setPen(QPen(QColor(0, 0, 0, 90), 1));
    p.setBrush(Qt::NoBrush);
    p.drawRoundedRect(QRectF(0.5, 0.5, 43, 27), 4, 4);
    return QIcon(pm);
}

// ------------------------------------------------------------------ tabs

QWidget *CustomizeDialog::buildThemeTab()
{
    auto *w = new QWidget;
    auto *h = new QHBoxLayout(w);

    auto *left = new QVBoxLayout;
    left->addWidget(new QLabel("<b>Themes</b>"));
    m_list = new QListWidget(w);
    m_list->setIconSize(QSize(44, 28));
    m_list->setMinimumWidth(200);
    left->addWidget(m_list, 1);
    auto *grid = new QGridLayout;
    auto *save = new QPushButton("Save as…", w);
    m_delete = new QPushButton("Delete", w);
    auto *imp = new QPushButton("Import…", w);
    auto *exp = new QPushButton("Export…", w);
    grid->addWidget(save, 0, 0);
    grid->addWidget(m_delete, 0, 1);
    grid->addWidget(imp, 1, 0);
    grid->addWidget(exp, 1, 1);
    left->addLayout(grid);
    h->addLayout(left);

    auto *scroll = new QScrollArea(w);
    scroll->setWidgetResizable(true);
    auto *colors = new QWidget;
    auto *cv = new QVBoxLayout(colors);
    QString group;
    QGridLayout *g = nullptr;
    int row = 0;
    for (const ThemeRole &r : themeRoles()) {
        if (r.group != group) {
            group = r.group;
            auto *box = new QGroupBox(group, colors);
            g = new QGridLayout(box);
            g->setColumnStretch(0, 1);
            cv->addWidget(box);
            row = 0;
        }
        auto *b = new QPushButton(colors);
        b->setFixedSize(64, 22);
        b->setToolTip("Click to change: " + r.label);
        connect(b, &QPushButton::clicked, this, [this, id = r.id] { editColor(id); });
        g->addWidget(new QLabel(r.label), row, 0);
        g->addWidget(b, row, 1);
        m_swatches[r.id] = b;
        ++row;
    }
    cv->addStretch();
    scroll->setWidget(colors);
    h->addWidget(scroll, 1);

    connect(m_list, &QListWidget::currentRowChanged, this, [this](int i) {
        if (m_loading || i < 0) return;
        QList<Theme> all = ThemeManager::instance().allThemes();
        if (i < all.size()) ThemeManager::instance().setTheme(all[i]);
        m_delete->setEnabled(i >= ThemeManager::presets().size());
    });
    connect(save, &QPushButton::clicked, this, [this] {
        auto &tm = ThemeManager::instance();
        QString base = tm.theme().name;
        if (base.endsWith(kModified)) base.chop(kModified.size());
        bool ok;
        QString name = QInputDialog::getText(this, "Save theme", "Theme name:", QLineEdit::Normal, base + " custom", &ok).trimmed();
        if (!ok || name.isEmpty()) return;
        for (const Theme &p : ThemeManager::presets())
            if (p.name == name) {
                QMessageBox::warning(this, "Save theme", "That name belongs to a built-in theme.");
                return;
            }
        Theme t = tm.theme();
        t.name = name;
        tm.saveCustom(t);
        tm.setTheme(t);
        refreshList();
    });
    connect(m_delete, &QPushButton::clicked, this, [this] {
        int i = m_list->currentRow();
        int presets = int(ThemeManager::presets().size());
        if (i < presets) return;
        auto &tm = ThemeManager::instance();
        tm.removeCustom(tm.customThemes().value(i - presets).name);
        tm.setTheme(ThemeManager::presets().first());
        refreshList();
    });
    connect(imp, &QPushButton::clicked, this, [this] {
        QString f = QFileDialog::getOpenFileName(this, "Import theme", {}, "Theme files (*.json);;All files (*)");
        if (f.isEmpty()) return;
        QFile file(f);
        if (!file.open(QIODevice::ReadOnly)) return;
        QJsonDocument d = QJsonDocument::fromJson(file.readAll());
        if (!d.isObject() || !d.object().contains("colors")) {
            QMessageBox::warning(this, "Import theme", "This file is not a SciCalc 991 theme.");
            return;
        }
        Theme t = Theme::fromJson(d.object());
        for (const Theme &p : ThemeManager::presets())
            if (p.name == t.name) t.name += " (imported)";
        ThemeManager::instance().saveCustom(t);
        ThemeManager::instance().setTheme(t);
        refreshList();
    });
    connect(exp, &QPushButton::clicked, this, [this] {
        const Theme &t = ThemeManager::instance().theme();
        QString f = QFileDialog::getSaveFileName(this, "Export theme", t.name + ".json", "Theme files (*.json)");
        if (f.isEmpty()) return;
        QFile file(f);
        if (file.open(QIODevice::WriteOnly)) file.write(QJsonDocument(t.toJson()).toJson());
    });
    return w;
}

QWidget *CustomizeDialog::buildDisplayTab()
{
    auto *w = new QWidget;
    auto *f = new QFormLayout(w);
    m_autoUiFont = new QCheckBox("System", w);
    m_uiFont = new QFontComboBox(w);
    auto *uiRow = new QHBoxLayout;
    uiRow->addWidget(m_autoUiFont);
    uiRow->addWidget(m_uiFont, 1);
    f->addRow("Interface font", uiRow);
    m_uiScale = makeSlider(70, 180, w);
    m_uiScaleLabel = new QLabel(w);
    auto *ur = new QHBoxLayout;
    ur->addWidget(m_uiScale, 1);
    ur->addWidget(m_uiScaleLabel);
    f->addRow("Interface size", ur);
    auto *uiNote = new QLabel("Keys, the input line, menus, tables and dialogs.", w);
    uiNote->setEnabled(false);
    f->addRow("", uiNote);

    m_autoFont = new QCheckBox("Automatic", w);
    m_font = new QFontComboBox(w);
    auto *fontRow = new QHBoxLayout;
    fontRow->addWidget(m_autoFont);
    fontRow->addWidget(m_font, 1);
    f->addRow("Math font", fontRow);
    m_mathScale = makeSlider(60, 200, w);
    m_mathScaleLabel = new QLabel(w);
    auto *sr = new QHBoxLayout;
    sr->addWidget(m_mathScale, 1);
    sr->addWidget(m_mathScaleLabel);
    f->addRow("Math size", sr);
    auto *mathNote = new QLabel("The rendered expressions and results on the display.", w);
    mathNote->setEnabled(false);
    f->addRow("", mathNote);
    m_history = new QCheckBox("Show calculation history", w);
    f->addRow("History", m_history);
    auto *note = new QLabel("Number format, angle unit, fractions and digit separators are in SETUP (SHIFT MENU).", w);
    note->setWordWrap(true);
    f->addRow(note);
    connect(m_autoFont, &QCheckBox::toggled, this, [this](bool on) { m_font->setEnabled(!on); pushAppearance(); });
    connect(m_font, &QFontComboBox::currentFontChanged, this, [this] { pushAppearance(); });
    connect(m_mathScale, &QSlider::valueChanged, this, [this] { pushAppearance(); });
    connect(m_autoUiFont, &QCheckBox::toggled, this, [this](bool on) { m_uiFont->setEnabled(!on); pushAppearance(); });
    connect(m_uiFont, &QFontComboBox::currentFontChanged, this, [this] { pushAppearance(); });
    connect(m_uiScale, &QSlider::valueChanged, this, [this] { pushAppearance(); });
    connect(m_history, &QCheckBox::toggled, this, [this] { pushAppearance(); });
    return w;
}

QWidget *CustomizeDialog::buildKeypadTab()
{
    auto *w = new QWidget;
    auto *f = new QFormLayout(w);
    m_keyScale = makeSlider(60, 160, w);
    m_keyScaleLabel = new QLabel(w);
    auto *r1 = new QHBoxLayout;
    r1->addWidget(m_keyScale, 1);
    r1->addWidget(m_keyScaleLabel);
    f->addRow("Key label size", r1);
    m_radius = makeSlider(0, 20, w);
    m_radiusLabel = new QLabel(w);
    auto *r2 = new QHBoxLayout;
    r2->addWidget(m_radius, 1);
    r2->addWidget(m_radiusLabel);
    f->addRow("Corner radius", r2);
    m_gradient = new QCheckBox("Glossy (gradient) keys", w);
    m_legends = new QCheckBox("Show SHIFT / ALPHA legends", w);
    m_keypad = new QCheckBox("Show keypad  (Ctrl+K toggles a compact, display-only window)", w);
    m_templates = new QComboBox(w);
    m_templates->addItems({"Letters  (a/b, \u221Ax, x^y, 10^x)", "Casio boxes  (template slots)"});
    f->addRow("Template symbols", m_templates);
    connect(m_templates, &QComboBox::currentIndexChanged, this, [this] { pushAppearance(); });
    f->addRow("Style", m_gradient);
    f->addRow("", m_legends);
    f->addRow("Layout", m_keypad);
    for (auto *s : {m_keyScale, m_radius}) connect(s, &QSlider::valueChanged, this, [this] { pushAppearance(); });
    for (auto *c : {m_gradient, m_legends, m_keypad}) connect(c, &QCheckBox::toggled, this, [this] { pushAppearance(); });
    return w;
}

QWidget *CustomizeDialog::buildWindowTab()
{
    auto *w = new QWidget;
    auto *f = new QFormLayout(w);
    m_brand = new QLineEdit(w);
    m_model = new QLineEdit(w);
    m_tagline = new QLineEdit(w);
    m_brand->setMaxLength(16);
    m_model->setMaxLength(10);
    m_tagline->setMaxLength(40);
    f->addRow("Calculator name", m_brand);
    f->addRow("Model", m_model);
    f->addRow("Tagline", m_tagline);
    m_onTop = new QCheckBox("Keep window on top", w);
    f->addRow("Window", m_onTop);
    m_opacity = makeSlider(30, 100, w);
    m_opacityLabel = new QLabel(w);
    auto *r = new QHBoxLayout;
    r->addWidget(m_opacity, 1);
    r->addWidget(m_opacityLabel);
    f->addRow("Opacity", r);
    for (auto *e : {m_brand, m_model, m_tagline}) connect(e, &QLineEdit::textChanged, this, [this] { pushAppearance(); });
    connect(m_onTop, &QCheckBox::toggled, this, [this] { pushAppearance(); });
    connect(m_opacity, &QSlider::valueChanged, this, [this] { pushAppearance(); });
    return w;
}

// ------------------------------------------------------------------ state sync

void CustomizeDialog::refreshList()
{
    m_loading = true;
    m_list->clear();
    const auto &tm = ThemeManager::instance();
    const QList<Theme> all = tm.allThemes();
    int presets = int(ThemeManager::presets().size());
    int current = -1;
    for (int i = 0; i < all.size(); ++i) {
        auto *it = new QListWidgetItem(themeIcon(all[i]), all[i].name + (i >= presets ? "  ★" : ""));
        m_list->addItem(it);
        if (all[i].name == tm.theme().name) current = i;
    }
    if (current >= 0) m_list->setCurrentRow(current);
    m_delete->setEnabled(current >= presets);
    m_loading = false;
}

void CustomizeDialog::refreshSwatches()
{
    const Theme &t = ThemeManager::instance().theme();
    for (auto it = m_swatches.begin(); it != m_swatches.end(); ++it) {
        QColor c = t.color(it.key());
        it.value()->setStyleSheet(QString("QPushButton { background:%1; border:1px solid %2; border-radius:3px; }")
                                      .arg(c.name(), c.lightness() > 128 ? "#555" : "#ccc"));
    }
    // keep the list selection in step (e.g. after Ctrl+T or a colour edit)
    if (!m_loading) {
        QList<Theme> all = ThemeManager::instance().allThemes();
        bool found = false;
        for (int i = 0; i < all.size(); ++i)
            if (all[i].name == t.name) {
                m_loading = true;
                m_list->setCurrentRow(i);
                m_loading = false;
                found = true;
            }
        if (!found) m_list->clearSelection();
    }
}

void CustomizeDialog::loadControls()
{
    m_loading = true;
    const Appearance &a = ThemeManager::instance().appearance();
    m_autoFont->setChecked(a.mathFont.isEmpty());
    m_font->setEnabled(!a.mathFont.isEmpty());
    if (!a.mathFont.isEmpty()) m_font->setCurrentFont(QFont(a.mathFont));
    m_mathScale->setValue(int(std::round(a.mathScale * 100)));
    m_autoUiFont->setChecked(a.uiFont.isEmpty());
    m_uiFont->setEnabled(!a.uiFont.isEmpty());
    if (!a.uiFont.isEmpty()) m_uiFont->setCurrentFont(QFont(a.uiFont));
    m_uiScale->setValue(int(std::round(a.uiScale * 100)));
    m_uiScaleLabel->setText(QString("%1%").arg(m_uiScale->value()));
    m_history->setChecked(a.showHistory);
    m_keyScale->setValue(int(std::round(a.keyScale * 100)));
    m_radius->setValue(a.keyRadius);
    m_gradient->setChecked(a.keyGradient);
    m_legends->setChecked(a.showLegends);
    m_templates->setCurrentIndex(a.letterTemplates ? 0 : 1);
    m_keypad->setChecked(a.showKeypad);
    m_brand->setText(a.brand);
    m_model->setText(a.model);
    m_tagline->setText(a.tagline);
    m_onTop->setChecked(a.alwaysOnTop);
    m_opacity->setValue(int(std::round(a.opacity * 100)));
    m_mathScaleLabel->setText(QString("%1%").arg(m_mathScale->value()));
    m_keyScaleLabel->setText(QString("%1%").arg(m_keyScale->value()));
    m_radiusLabel->setText(QString("%1 px").arg(m_radius->value()));
    m_opacityLabel->setText(QString("%1%").arg(m_opacity->value()));
    m_loading = false;
    refreshList();
    refreshSwatches();
}

void CustomizeDialog::pushAppearance()
{
    m_mathScaleLabel->setText(QString("%1%").arg(m_mathScale->value()));
    m_uiScaleLabel->setText(QString("%1%").arg(m_uiScale->value()));
    m_keyScaleLabel->setText(QString("%1%").arg(m_keyScale->value()));
    m_radiusLabel->setText(QString("%1 px").arg(m_radius->value()));
    m_opacityLabel->setText(QString("%1%").arg(m_opacity->value()));
    if (m_loading) return;
    Appearance a;
    a.mathFont = m_autoFont->isChecked() ? QString() : m_font->currentFont().family();
    a.mathScale = m_mathScale->value() / 100.0;
    a.uiFont = m_autoUiFont->isChecked() ? QString() : m_uiFont->currentFont().family();
    a.uiScale = m_uiScale->value() / 100.0;
    a.showHistory = m_history->isChecked();
    a.keyScale = m_keyScale->value() / 100.0;
    a.keyRadius = m_radius->value();
    a.keyGradient = m_gradient->isChecked();
    a.showLegends = m_legends->isChecked();
    a.letterTemplates = m_templates->currentIndex() == 0;
    a.showKeypad = m_keypad->isChecked();
    a.brand = m_brand->text();
    a.model = m_model->text();
    a.tagline = m_tagline->text();
    a.alwaysOnTop = m_onTop->isChecked();
    a.opacity = m_opacity->value() / 100.0;
    if (a == ThemeManager::instance().appearance()) return;
    ThemeManager::instance().setAppearance(a);
}

void CustomizeDialog::editColor(const QString &role)
{
    auto &tm = ThemeManager::instance();
    Theme t = tm.theme();
    QString label = role;
    for (const auto &r : themeRoles())
        if (r.id == role) label = r.label;
    QColor c = QColorDialog::getColor(t.color(role), this, "Choose colour: " + label);
    if (!c.isValid()) return;
    t.colors[role] = c;
    if (!t.name.endsWith(kModified)) t.name += kModified;
    tm.setTheme(t);
}

void CustomizeDialog::accept()
{
    ThemeManager::instance().save();
    QDialog::accept();
}

void CustomizeDialog::reject()
{
    auto &tm = ThemeManager::instance();
    tm.setTheme(m_origTheme);
    tm.setAppearance(m_origApp);
    QDialog::reject();
}
