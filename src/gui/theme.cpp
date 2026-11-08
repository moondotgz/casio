#include "theme.h"
#include <QApplication>
#include <QJsonArray>
#include <QRegularExpression>
#include <QJsonDocument>
#include <QSettings>

const QList<ThemeRole> &themeRoles()
{
    static const QList<ThemeRole> roles = {
        {"bodyTop", "Body (top)", "Body"},
        {"bodyBottom", "Body (bottom)", "Body"},
        {"bezel", "Display bezel", "Body"},
        {"brand", "Brand text", "Body"},
        {"brandDim", "Brand secondary", "Body"},
        {"lcdBg", "Background", "Display"},
        {"lcdText", "Text / ink", "Display"},
        {"lcdDim", "Hints", "Display"},
        {"lcdLine", "Lines & grid", "Display"},
        {"lcdPanel", "Input fields", "Display"},
        {"lcdAccent", "Accent / selection", "Display"},
        {"lcdAccentText", "Text on accent", "Display"},
        {"lcdError", "Error text", "Display"},
        {"keyFunc", "Function keys", "Keys"},
        {"keyFuncText", "Function key text", "Keys"},
        {"keyNum", "Number keys", "Keys"},
        {"keyNumText", "Number key text", "Keys"},
        {"keyAccent", "DEL / AC keys", "Keys"},
        {"keyAccentText", "DEL / AC text", "Keys"},
        {"keyShift", "SHIFT key", "Keys"},
        {"keyShiftText", "SHIFT key text", "Keys"},
        {"keyAlpha", "ALPHA key", "Keys"},
        {"keyAlphaText", "ALPHA key text", "Keys"},
        {"legendShift", "SHIFT legends", "Keys"},
        {"legendAlpha", "ALPHA legends", "Keys"},
        {"navPad", "Cursor pad", "Keys"},
        {"navArrow", "Cursor arrows", "Keys"},
    };
    return roles;
}

namespace {

Theme make(const QString &name, std::initializer_list<const char *> hex)
{
    Theme t;
    t.name = name;
    const auto &roles = themeRoles();
    int i = 0;
    for (const char *h : hex) {
        if (i < roles.size()) t.colors[roles[i].id] = QColor(QString::fromLatin1(h));
        ++i;
    }
    Q_ASSERT(i == roles.size());
    return t;
}

QString hex(const QColor &c) { return c.name(QColor::HexRgb); }

} // namespace

const QList<Theme> &ThemeManager::presets()
{
    static const QList<Theme> list = {
        make("Classic", {"#30343b", "#181a1e", "#101114", "#f2f3f5", "#8e959e",
                         "#dfe4d8", "#1a1d19", "#4b5146", "#b5bcac", "#eef1ea", "#2f3a2c", "#eef2ea", "#a81e1e",
                         "#454a52", "#f0f1f3", "#dcdee2", "#191b1e", "#4479c4", "#ffffff", "#dca23c", "#281c06",
                         "#cc5274", "#ffffff", "#f0b54a", "#f07096", "#b4b8c0", "#2d3036"}),
        make("Midnight", {"#1d2130", "#0c0e14", "#05060a", "#e6edf3", "#7d8590",
                          "#0d1117", "#c9d1d9", "#8b949e", "#30363d", "#161b22", "#1f6feb", "#ffffff", "#f85149",
                          "#232a38", "#c9d1d9", "#2f3747", "#e6edf3", "#1f6feb", "#ffffff", "#d29922", "#1a1300",
                          "#bf4b8a", "#ffffff", "#e3b341", "#f778ba", "#3b4252", "#c9d1d9"}),
        make("Retro LCD", {"#454b38", "#272b1f", "#1a1d14", "#e8e4cf", "#a7a58c",
                           "#9fb56a", "#1d2f12", "#3c5427", "#7e9550", "#b3c77f", "#2b4419", "#c9dba0", "#6a1a10",
                           "#5c5a4c", "#efe9d2", "#d9d2bb", "#2b2a22", "#9e3b2c", "#fff3e6", "#c8952f", "#231803",
                           "#7c6a9c", "#ffffff", "#e3b45a", "#c9a7e8", "#b8b29a", "#3a382e"}),
        make("Ocean", {"#0f4a6a", "#06202e", "#03141d", "#e6f6fb", "#7fb3c8",
                       "#e6f4f8", "#0b2a3a", "#3f6a7d", "#a9cad6", "#f5fbfd", "#0e7490", "#ffffff", "#c2410c",
                       "#14587a", "#e6f6fb", "#eaf3f6", "#0b2a3a", "#f97316", "#ffffff", "#f5b82e", "#2a1a00",
                       "#e0609a", "#ffffff", "#fbbf24", "#f9a8d4", "#8fb9c9", "#0b3448"}),
        make("Rosé", {"#4e2e3d", "#24141b", "#170c11", "#fde7ee", "#c28fa3",
                           "#fdf0f3", "#3a1f2b", "#8a5a6c", "#e7c3cf", "#fff8fa", "#b4436c", "#ffffff", "#b91c1c",
                           "#6b3a50", "#fde7ee", "#f8e6ec", "#3a1f2b", "#b4436c", "#ffffff", "#efb34a", "#2b1a00",
                           "#d9668f", "#ffffff", "#f2c14e", "#ff9dbd", "#d7b3c1", "#4a2535"}),
        make("Nord", {"#3b4252", "#242933", "#1d2129", "#eceff4", "#a3abba",
                      "#eceff4", "#2e3440", "#4c566a", "#d8dee9", "#f8f9fb", "#5e81ac", "#ffffff", "#bf616a",
                      "#4c566a", "#eceff4", "#d8dee9", "#2e3440", "#bf616a", "#ffffff", "#ebcb8b", "#2e3440",
                      "#b48ead", "#ffffff", "#ebcb8b", "#d4b2d0", "#a7b1c2", "#2e3440"}),
        make("Solarized", {"#0a4352", "#002129", "#00171d", "#eee8d5", "#93a1a1",
                           "#fdf6e3", "#073642", "#657b83", "#e0d8bf", "#fffbef", "#268bd2", "#ffffff", "#dc322f",
                           "#0d5263", "#eee8d5", "#eee8d5", "#073642", "#cb4b16", "#ffffff", "#b58900", "#fdf6e3",
                           "#d33682", "#ffffff", "#d6a400", "#f06aa8", "#93a1a1", "#073642"}),
        make("Paper", {"#f4f2ed", "#d9d5cb", "#bdb7ab", "#2a2a2a", "#7a756b",
                       "#ffffff", "#141414", "#6b6b6b", "#dddddd", "#f6f6f6", "#2563eb", "#ffffff", "#c81e1e",
                       "#e6e2d9", "#262626", "#ffffff", "#141414", "#2563eb", "#ffffff", "#d97706", "#ffffff",
                       "#db2777", "#ffffff", "#b45309", "#be185d", "#cfcac0", "#3a3a3a"}),
        make("Amber Terminal", {"#1f1f1f", "#090909", "#000000", "#ffcf66", "#9c7a3c",
                                "#140e04", "#ffb000", "#b07a00", "#4a3300", "#1e1607", "#ffb000", "#140e04", "#ff5c5c",
                                "#2a2a2a", "#ffcf66", "#383838", "#ffd27a", "#b45309", "#fff1d6", "#ffb000", "#1a1000",
                                "#c2410c", "#fff1d6", "#ffb000", "#ff8a5c", "#3c3c3c", "#ffb000"}),
        make("High Contrast", {"#000000", "#000000", "#000000", "#ffffff", "#ffff00",
                               "#000000", "#ffffff", "#ffff00", "#ffffff", "#000000", "#ffff00", "#000000", "#ff4040",
                               "#000000", "#ffffff", "#ffffff", "#000000", "#ffff00", "#000000", "#00ffff", "#000000",
                               "#ff00ff", "#000000", "#00ffff", "#ff66ff", "#ffffff", "#000000"}),
    };
    return list;
}

QColor Theme::color(const QString &role) const
{
    auto it = colors.find(role);
    if (it != colors.end()) return *it;
    const Theme &classic = ThemeManager::presets().first();
    return &classic == this ? QColor(Qt::magenta) : classic.colors.value(role, Qt::magenta);
}

QJsonObject Theme::toJson() const
{
    QJsonObject c;
    for (const auto &r : themeRoles()) c[r.id] = hex(color(r.id));
    return QJsonObject{{"name", name}, {"format", "scicalc991-theme"}, {"colors", c}};
}

Theme Theme::fromJson(const QJsonObject &o)
{
    Theme t = ThemeManager::presets().first();
    t.name = o.value("name").toString("Imported");
    const QJsonObject c = o.value("colors").toObject();
    for (const auto &r : themeRoles()) {
        QColor col(c.value(r.id).toString());
        if (col.isValid()) t.colors[r.id] = col;
    }
    return t;
}

// ------------------------------------------------------------------ manager

ThemeManager &ThemeManager::instance()
{
    static ThemeManager m;
    return m;
}

ThemeManager::ThemeManager() : m_theme(presets().first()) {}

void ThemeManager::setTheme(const Theme &t)
{
    m_theme = t;
    emit changed();
}

void ThemeManager::setAppearance(const Appearance &a)
{
    m_app = a;
    m_app.mathScale = std::clamp(m_app.mathScale, 0.6, 2.0);
    m_app.keyScale = std::clamp(m_app.keyScale, 0.6, 1.6);
    m_app.keyRadius = std::clamp(m_app.keyRadius, 0, 20);
    m_app.opacity = std::clamp(m_app.opacity, 0.3, 1.0);
    m_app.uiScale = std::clamp(m_app.uiScale, 0.7, 1.8);
    emit changed();
}

void ThemeManager::saveCustom(const Theme &t)
{
    for (auto &c : m_custom)
        if (c.name == t.name) {
            c = t;
            save();
            return;
        }
    m_custom.push_back(t);
    save();
}

void ThemeManager::removeCustom(const QString &name)
{
    m_custom.erase(std::remove_if(m_custom.begin(), m_custom.end(), [&](const Theme &t) { return t.name == name; }),
                   m_custom.end());
    save();
}

void ThemeManager::cycleTheme(int step)
{
    QList<Theme> all = allThemes();
    int idx = 0;
    for (int i = 0; i < all.size(); ++i)
        if (all[i].name == m_theme.name) idx = i;
    setTheme(all[(idx + step + all.size()) % all.size()]);
    save();
}

QString ThemeManager::lcdStyleSheet() const
{
    QString s = QStringLiteral(R"(
        QWidget#lcd { background: @bg; border-radius: 6px; }
        QWidget#lcd QLabel { color: @text; background: transparent; }
        QWidget#lcd QLabel#lcdTitle { font-weight: bold; font-size: 14px; }
        QWidget#lcd QLabel#lcdText { font-size: 13px; }
        QWidget#lcd QLabel#errorBox { color: @bg; background: @text; padding: 6px 10px; border-radius: 4px; font-weight: bold; }
        QWidget#lcd QLabel#hint { color: @dim; font-size: 11px; }
        QLineEdit#exprEdit, QWidget#lcd QLineEdit {
            background: @panel; color: @text; border: 1px solid @line; border-radius: 3px;
            padding: 3px 5px; font-size: 15px; selection-background-color: @accent; selection-color: @accentText; }
        QLineEdit#exprEdit:focus { border: 1px solid @accent; }
        QWidget#lcd QTableWidget, QWidget#lcd QTableView {
            background: @panel; color: @text; gridline-color: @line; border: 1px solid @line;
            selection-background-color: @accent; selection-color: @accentText; font-size: 13px; }
        QWidget#lcd QHeaderView::section { background: @bg; color: @text; border: 0; border-right: 1px solid @line;
            border-bottom: 1px solid @line; padding: 2px 4px; font-weight: bold; }
        QWidget#lcd QTableCornerButton::section { background: @bg; border: 0; }
        QWidget#lcd QComboBox { background: @panel; color: @text; border: 1px solid @line; border-radius: 3px; padding: 2px 6px; }
        QWidget#lcd QComboBox QAbstractItemView { background: @panel; color: @text; selection-background-color: @accent;
            selection-color: @accentText; }
        QWidget#lcd QPushButton { background: @panel; color: @text; border: 1px solid @line; border-radius: 3px; padding: 3px 10px; }
        QWidget#lcd QPushButton:hover { border: 1px solid @accent; }
        QWidget#lcd QPushButton:checked { background: @accent; color: @accentText; }
        QWidget#lcd QCheckBox, QWidget#lcd QRadioButton { color: @text; background: transparent; }
        QWidget#lcd QScrollArea { background: transparent; border: 0; }
        QWidget#lcd QScrollArea > QWidget > QWidget { background: transparent; }
        QWidget#lcd QTabWidget::pane { border: 1px solid @line; top: -1px; }
        QWidget#lcd QTabBar::tab { background: @bg; color: @text; padding: 3px 12px; border: 1px solid @line;
            border-bottom: 0; border-top-left-radius: 3px; border-top-right-radius: 3px; }
        QWidget#lcd QTabBar::tab:selected { background: @panel; font-weight: bold; }
        QWidget#lcd QSpinBox { background: @panel; color: @text; border: 1px solid @line; }
        QWidget#lcd QScrollBar:vertical { background: @bg; width: 10px; margin: 0; border: 0; }
        QWidget#lcd QScrollBar::handle:vertical { background: @line; min-height: 24px; border-radius: 4px; margin: 1px; }
        QWidget#lcd QScrollBar:horizontal { background: @bg; height: 10px; margin: 0; border: 0; }
        QWidget#lcd QScrollBar::handle:horizontal { background: @line; min-width: 24px; border-radius: 4px; margin: 1px; }
        QWidget#lcd QScrollBar::add-line, QWidget#lcd QScrollBar::sub-line { width: 0; height: 0; }
        QWidget#lcd QScrollBar::add-page, QWidget#lcd QScrollBar::sub-page { background: none; }
        QFrame#sep { background: @line; max-height: 1px; min-height: 1px; }
        QWidget#historyEntry { border-bottom: 1px dotted @line; }
    )");
    // longest tokens first so "@accentText" is not clobbered by "@accent"
    s.replace("@accentText", hex(color("lcdAccentText")));
    s.replace("@accent", hex(color("lcdAccent")));
    s.replace("@bg", hex(color("lcdBg")));
    s.replace("@text", hex(color("lcdText")));
    s.replace("@dim", hex(color("lcdDim")));
    s.replace("@line", hex(color("lcdLine")));
    s.replace("@panel", hex(color("lcdPanel")));
    // scale the fixed pixel sizes with the interface size
    static const QRegularExpression px("font-size: (\\d+)px");
    QString out;
    qsizetype last = 0;
    for (auto it = px.globalMatch(s); it.hasNext();) {
        auto m = it.next();
        out += s.mid(last, m.capturedStart() - last);
        out += QString("font-size: %1px").arg(std::max(7, int(std::round(m.captured(1).toInt() * m_app.uiScale))));
        last = m.capturedEnd();
    }
    out += s.mid(last);
    return out;
}

QFont ThemeManager::interfaceFont() const
{
    QFont f = m_haveSystemFont ? m_systemFont : QApplication::font();
    if (!m_app.uiFont.isEmpty()) f.setFamilies({m_app.uiFont});
    if (f.pointSizeF() > 0) f.setPointSizeF(f.pointSizeF() * m_app.uiScale);
    else if (f.pixelSize() > 0) f.setPixelSize(std::max(6, int(std::round(f.pixelSize() * m_app.uiScale))));
    return f;
}

QString ThemeManager::menuStyleSheet() const
{
    return QString("QMenu { background:%1; color:%2; border:1px solid %3; font-size:13px; }"
                   "QMenu::item { padding:4px 22px 4px 12px; }"
                   "QMenu::item:selected { background:%4; color:%5; }"
                   "QMenu::separator { height:1px; background:%3; }")
        .arg(hex(color("lcdPanel")), hex(color("lcdText")), hex(color("lcdLine")), hex(color("lcdAccent")),
             hex(color("lcdAccentText")));
}

QPalette ThemeManager::dialogPalette() const
{
    QPalette p;
    QColor bg = color("lcdBg"), text = color("lcdText"), panel = color("lcdPanel");
    QColor button = QColor::fromRgbF((bg.redF() * 2 + color("lcdLine").redF()) / 3,
                                     (bg.greenF() * 2 + color("lcdLine").greenF()) / 3,
                                     (bg.blueF() * 2 + color("lcdLine").blueF()) / 3);
    p.setColor(QPalette::Window, bg);
    p.setColor(QPalette::WindowText, text);
    p.setColor(QPalette::Base, panel);
    p.setColor(QPalette::AlternateBase, bg);
    p.setColor(QPalette::Text, text);
    p.setColor(QPalette::Button, button);
    p.setColor(QPalette::ButtonText, text);
    p.setColor(QPalette::BrightText, color("lcdError"));
    p.setColor(QPalette::Highlight, color("lcdAccent"));
    p.setColor(QPalette::HighlightedText, color("lcdAccentText"));
    p.setColor(QPalette::ToolTipBase, panel);
    p.setColor(QPalette::ToolTipText, text);
    p.setColor(QPalette::PlaceholderText, color("lcdDim"));
    p.setColor(QPalette::Link, color("lcdAccent"));
    p.setColor(QPalette::Mid, color("lcdLine"));
    p.setColor(QPalette::Dark, color("lcdLine").darker(130));
    p.setColor(QPalette::Light, panel.lighter(110));
    for (auto role : {QPalette::WindowText, QPalette::Text, QPalette::ButtonText})
        p.setColor(QPalette::Disabled, role, color("lcdDim"));
    return p;
}

// ------------------------------------------------------------------ persistence

void ThemeManager::load()
{
    if (!m_haveSystemFont) {   // remember the untouched application font before any customisation
        m_systemFont = QApplication::font();
        m_haveSystemFont = true;
    }
    QSettings q("SciCalc991", "SciCalc991");
    q.beginGroup("appearance");
    const QJsonArray customs = QJsonDocument::fromJson(q.value("customThemes").toByteArray()).array();
    m_custom.clear();
    for (const auto &v : customs) m_custom.push_back(Theme::fromJson(v.toObject()));
    const QJsonDocument cur = QJsonDocument::fromJson(q.value("theme").toByteArray());
    if (cur.isObject()) m_theme = Theme::fromJson(cur.object());
    Appearance a;
    a.uiFont = q.value("uiFont", a.uiFont).toString();
    a.uiScale = q.value("uiScale", a.uiScale).toDouble();
    a.mathFont = q.value("mathFont", a.mathFont).toString();
    a.mathScale = q.value("mathScale", a.mathScale).toDouble();
    a.keyScale = q.value("keyScale", a.keyScale).toDouble();
    a.keyRadius = q.value("keyRadius", a.keyRadius).toInt();
    a.keyGradient = q.value("keyGradient", a.keyGradient).toBool();
    a.showLegends = q.value("showLegends", a.showLegends).toBool();
    a.letterTemplates = q.value("letterTemplates", a.letterTemplates).toBool();
    a.showKeypad = q.value("showKeypad", a.showKeypad).toBool();
    a.showHistory = q.value("showHistory", a.showHistory).toBool();
    a.alwaysOnTop = q.value("alwaysOnTop", a.alwaysOnTop).toBool();
    a.opacity = q.value("opacity", a.opacity).toDouble();
    a.brand = q.value("brand", a.brand).toString();
    a.model = q.value("model", a.model).toString();
    a.tagline = q.value("tagline", a.tagline).toString();
    q.endGroup();
    m_app = a;
    setAppearance(a);   // clamps and notifies
}

void ThemeManager::save() const
{
    QSettings q("SciCalc991", "SciCalc991");
    q.beginGroup("appearance");
    q.setValue("theme", QJsonDocument(m_theme.toJson()).toJson(QJsonDocument::Compact));
    QJsonArray customs;
    for (const auto &t : m_custom) customs.append(t.toJson());
    q.setValue("customThemes", QJsonDocument(customs).toJson(QJsonDocument::Compact));
    const Appearance &a = m_app;
    q.setValue("uiFont", a.uiFont);
    q.setValue("uiScale", a.uiScale);
    q.setValue("mathFont", a.mathFont);
    q.setValue("mathScale", a.mathScale);
    q.setValue("keyScale", a.keyScale);
    q.setValue("keyRadius", a.keyRadius);
    q.setValue("keyGradient", a.keyGradient);
    q.setValue("showLegends", a.showLegends);
    q.setValue("letterTemplates", a.letterTemplates);
    q.setValue("showKeypad", a.showKeypad);
    q.setValue("showHistory", a.showHistory);
    q.setValue("alwaysOnTop", a.alwaysOnTop);
    q.setValue("opacity", a.opacity);
    q.setValue("brand", a.brand);
    q.setValue("model", a.model);
    q.setValue("tagline", a.tagline);
    q.endGroup();
}
