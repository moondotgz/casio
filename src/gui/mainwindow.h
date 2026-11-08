#pragma once
#include "keypad.h"
#include "modepage.h"
#include <QWidget>

class QStackedWidget;
class QLabel;
class QTimer;
class QToolButton;
class MenuPage;

class MainWindow : public QWidget {
    Q_OBJECT
public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow() override;
    // Test/automation hook: '|'-separated steps such as "key:7", "key:shift", "type:sin(30)",
    // "act:c:EXE", "nav:UP", "mode:3".
    void runScript(const QString &script);

protected:
    void paintEvent(QPaintEvent *) override;
    void closeEvent(QCloseEvent *) override;

private:
    AppState m_state;
    Keypad *m_keypad;
    QWidget *m_lcd;
    QStackedWidget *m_pages;
    MenuPage *m_menu;
    QList<ModePage *> m_modes;
    int m_mode = 0;
    QLabel *m_ind[8];
    QLabel *m_modeLabel;
    QLabel *m_brand, *m_tagline;
    QToolButton *m_gear;
    bool m_keypadShown = true;
    bool m_onTop = false;
    QLabel *m_toast;
    QTimer *m_toastTimer;
    bool m_shift = false, m_alpha = false, m_sto = false;

    ModePage *page() const;
    bool menuShown() const;
    void onKey(const KeyDef &def);
    void onNav(const QString &dir);
    void dispatch(const QString &action);
    void showMenu();
    void selectMode(int index);
    void popup(const QList<MenuItem> &items);
    void openSetup();
    void openRecall();
    void openReset();
    void openCustomize();
    void applyTheme();
    void settingsChanged();
    void updateStatus();
    void updateLayer();
    void toast(const QString &text);
    void loadState();
    void saveState();
};
