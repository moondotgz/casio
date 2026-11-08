#pragma once
#include <QColor>
#include <QFont>
#include <QJsonObject>
#include <QMap>
#include <QObject>
#include <QPalette>
#include <QStringList>

// A colour scheme: role name → colour. Roles are listed by themeRoles().
struct Theme {
    QString name;
    QMap<QString, QColor> colors;

    QColor color(const QString &role) const;
    QJsonObject toJson() const;
    static Theme fromJson(const QJsonObject &o);   // missing roles fall back to Classic
};

struct ThemeRole {
    QString id;
    QString label;
    QString group;
};
const QList<ThemeRole> &themeRoles();

// Non-colour customisation.
struct Appearance {
    QString uiFont;            // interface font (keys, input, menus, tables); empty = system
    double uiScale = 1.0;      // interface text size factor
    QString mathFont;          // empty = automatic
    double mathScale = 1.0;    // natural-display size factor
    double keyScale = 1.0;     // key label size factor
    int keyRadius = 7;         // key corner radius (px)
    bool keyGradient = true;   // glossy vs flat keys
    bool showLegends = true;   // SHIFT / ALPHA legends above keys
    bool letterTemplates = true;   // key labels √x, xʸ … instead of Casio-style √□, x□
    bool showKeypad = true;
    bool showHistory = true;
    bool alwaysOnTop = false;
    double opacity = 1.0;
    QString brand = "SCICALC";
    QString model = "991";
    QString tagline = "scientific calculator";

    bool operator==(const Appearance &) const = default;
};

class ThemeManager : public QObject {
    Q_OBJECT
public:
    static ThemeManager &instance();

    const Theme &theme() const { return m_theme; }
    const Appearance &appearance() const { return m_app; }
    QColor color(const QString &role) const { return m_theme.color(role); }

    static const QList<Theme> &presets();
    QList<Theme> customThemes() const { return m_custom; }
    QList<Theme> allThemes() const { return presets() + m_custom; }

    void setTheme(const Theme &t);
    void setAppearance(const Appearance &a);
    void saveCustom(const Theme &t);
    void removeCustom(const QString &name);
    void cycleTheme(int step);

    QString lcdStyleSheet() const;
    QFont interfaceFont() const;   // system font with the uiFont / uiScale customisation applied
    QString menuStyleSheet() const;
    QPalette dialogPalette() const;

    void load();
    void save() const;

signals:
    void changed();

private:
    ThemeManager();
    Theme m_theme;
    Appearance m_app;
    QList<Theme> m_custom;
    QFont m_systemFont;
    bool m_haveSystemFont = false;
};

inline QColor themeColor(const QString &role) { return ThemeManager::instance().color(role); }
inline const Appearance &appearance() { return ThemeManager::instance().appearance(); }
