#pragma once
#include "theme.h"
#include <QDialog>
#include <QMap>

class QListWidget;
class QPushButton;
class QFontComboBox;
class QCheckBox;
class QSlider;
class QLabel;
class QLineEdit;
class QComboBox;

// Live-preview editor for themes and appearance. Cancel restores the previous look.
class CustomizeDialog : public QDialog {
    Q_OBJECT
public:
    explicit CustomizeDialog(QWidget *parent = nullptr);
    void accept() override;
    void reject() override;
    static QIcon themeIcon(const Theme &t);

private:
    Theme m_origTheme;
    Appearance m_origApp;
    bool m_loading = false;

    QListWidget *m_list;
    QPushButton *m_delete;
    QMap<QString, QPushButton *> m_swatches;
    // display
    QCheckBox *m_autoFont, *m_history, *m_autoUiFont;
    QFontComboBox *m_font, *m_uiFont;
    QSlider *m_mathScale, *m_uiScale;
    QLabel *m_mathScaleLabel, *m_uiScaleLabel;
    // keypad
    QSlider *m_keyScale, *m_radius;
    QLabel *m_keyScaleLabel, *m_radiusLabel;
    QCheckBox *m_gradient, *m_legends, *m_keypad;
    QComboBox *m_templates;
    // window
    QLineEdit *m_brand, *m_model, *m_tagline;
    QCheckBox *m_onTop;
    QSlider *m_opacity;
    QLabel *m_opacityLabel;

    QWidget *buildThemeTab();
    QWidget *buildDisplayTab();
    QWidget *buildKeypadTab();
    QWidget *buildWindowTab();
    void refreshList();
    void refreshSwatches();
    void loadControls();
    void pushAppearance();
    void editColor(const QString &role);
};
