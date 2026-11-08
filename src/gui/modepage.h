#pragma once
#include "core/evaluator.h"
#include "core/format.h"
#include "mathview.h"
#include "richtext.h"
#include <QLineEdit>
#include <QList>
#include <QStyledItemDelegate>
#include <QWidget>

class QLabel;
class QTableWidget;

struct AppState {
    Context ctx;
    Settings &settings() { return ctx.settings; }
};

struct MenuItem {
    QString label;
    QString insert;            // "i:..." / "c:..." / plain text to insert
    QList<MenuItem> children;
    MenuItem(QString l = {}, QString ins = {}, QList<MenuItem> ch = {})
        : label(std::move(l)), insert(std::move(ins)), children(std::move(ch)) {}
};

// Line edit for calculator expressions: token-aware delete, template insertion (§ = cursor).
class ExprEdit : public QLineEdit {
    Q_OBJECT
public:
    explicit ExprEdit(QWidget *parent = nullptr);
    void insertTemplate(const QString &t);
    void smartBackspace();
signals:
    void upPressed();
    void downPressed();
    void typedWhileFresh(const QString &text);   // printable key typed right after a result
protected:
    void keyPressEvent(QKeyEvent *e) override;
public:
    bool fresh = false;   // true right after "=" (next input starts a new calculation)
};

class ModePage : public QWidget {
    Q_OBJECT
public:
    explicit ModePage(AppState *st, QWidget *parent = nullptr);

    virtual QString modeName() const = 0;
    virtual QString indicator() const { return {}; }
    virtual void execute(bool approx) { Q_UNUSED(approx); }
    virtual void allClear();
    virtual void del();
    virtual void navigate(const QString &dir);   // LEFT RIGHT UP DOWN
    virtual void command(const QString &cmd) { Q_UNUSED(cmd); }
    virtual bool handleKey(const QString &keyId) { Q_UNUSED(keyId); return false; }
    virtual QList<MenuItem> optnMenu() const;
    virtual void insertText(const QString &text, bool startsNew);
    virtual void activated() {}
    virtual void settingsChanged() {}
    virtual QWidget *editor() const { return nullptr; }

    // Evaluate an expression string in this page's context (complex flag etc. applied).
    Value evalString(const QString &src, bool allowMulti = false);
    // Prepare the shared context for this mode (call before evaluating).
    virtual void prepareContext();

    static QString styleSheet();

signals:
    void statusChanged();
    void message(const QString &text);

protected:
    AppState *st;
    Context &ctx() { return st->ctx; }
    const Settings &settings() const { return st->ctx.settings; }
    MathStyle mathStyle(qreal size = 20) const;
    ExprEdit *focusedEdit() const;
    void showError(QLabel *label, const CalcError &e);

    // helpers for building forms
    static QLabel *lcdLabel(const QString &text, QWidget *parent, bool bold = false);
    QList<MenuItem> calcMenuItems() const;
};

// Item delegate that edits table cells with an ExprEdit (so the keypad can type into cells).
class ExprDelegate : public RichDelegate {
    Q_OBJECT
public:
    using RichDelegate::RichDelegate;
    QWidget *createEditor(QWidget *parent, const QStyleOptionViewItem &, const QModelIndex &) const override;
};

// Commit any cell editor currently open in the table.
void flushTableEditor(QTableWidget *t);
// Open an ExprEdit on the current cell (replacing its text) unless one is already open.
void beginCellInput(QTableWidget *t);
// Move the current cell with the cursor keys (commits an open editor first).
void navigateTable(QTableWidget *t, const QString &dir);

// Common OPTN sub-menus
QList<MenuItem> hyperbolicMenu();
QList<MenuItem> angleUnitMenu();
QList<MenuItem> numericMenu();
QList<MenuItem> catalogMenu();
QList<MenuItem> constantsMenu();
QList<MenuItem> conversionsMenu();
