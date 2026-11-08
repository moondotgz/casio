#pragma once
#include "modepage.h"
#include <optional>

class QLabel;
class QScrollArea;
class QStackedWidget;
class QVBoxLayout;
class QFormLayout;
class QComboBox;
class QSpinBox;
class QTableWidget;
class QPushButton;
class QButtonGroup;

enum class CalcKind { Calculate, Complex, BaseN, Matrix, Vector };

struct CalcEntry {
    QString input;
    bool error = false;
    CalcError err{ErrKind::Syntax};
    Value value;
    QList<QPair<QString, Value>> outputs;
    DisplayOpts opts;
    QString special;     // overrides the value display (FACT)
    QString note;        // e.g. "→A", "M+"
    int base = 0;
};

class CalcPage : public ModePage {
    Q_OBJECT
public:
    CalcPage(CalcKind kind, AppState *st, QWidget *parent = nullptr);

    QString modeName() const override;
    QString indicator() const override;
    void execute(bool approx) override;
    void allClear() override;
    void del() override;
    void navigate(const QString &dir) override;
    void command(const QString &cmd) override;
    bool handleKey(const QString &keyId) override;
    QList<MenuItem> optnMenu() const override;
    void insertText(const QString &text, bool startsNew) override;
    void activated() override;
    void settingsChanged() override;
    QWidget *editor() const override;
    void prepareContext() override;

private:
    enum class Prompt { None, Calc, Solve };

    CalcKind m_kind;
    int m_base = 10;

    QStackedWidget *m_stack = nullptr;
    // calculation view
    QScrollArea *m_scroll = nullptr;
    QWidget *m_historySpacer = nullptr;
    QWidget *m_historyHost = nullptr;
    QVBoxLayout *m_historyLayout = nullptr;
    MathView *m_preview = nullptr;
    MathView *m_result = nullptr;
    QLabel *m_error = nullptr;
    QLabel *m_baseInfo = nullptr;
    QLabel *m_note = nullptr;
    ExprEdit *m_edit = nullptr;
    QButtonGroup *m_baseButtons = nullptr;
    QList<QPushButton *> m_defineButtons;

    // CALC / SOLVE prompt view
    Prompt m_prompt = Prompt::None;
    QWidget *m_promptPanel = nullptr;
    QLabel *m_promptTitle = nullptr;
    MathView *m_promptExpr = nullptr;
    QFormLayout *m_promptForm = nullptr;
    QComboBox *m_solveVar = nullptr;
    QList<QPair<QString, ExprEdit *>> m_promptFields;
    QString m_promptSource;

    // matrix / vector editor view
    QWidget *m_editorPanel = nullptr;
    QLabel *m_editorTitle = nullptr;
    QSpinBox *m_rows = nullptr, *m_cols = nullptr;
    QTableWidget *m_table = nullptr;
    QString m_editing;

    QList<CalcEntry> m_entries;
    std::optional<CalcEntry> m_current;
    int m_recall = -1;

    QWidget *buildCalcView();
    QWidget *buildToolbar();
    QWidget *buildPromptPanel();
    QWidget *buildEditorPanel();

    void updatePreview();
    void renderCurrent();
    BoxPtr resultBox(const CalcEntry &e, qreal size);
    void archiveCurrent();
    void addHistoryWidget(const CalcEntry &e);
    void rebuildHistory();
    void evaluateInto(CalcEntry &e, bool approx);
    void runExpression(const QString &text, bool approx);
    bool ensureEvaluated();
    void storeTo(const QString &var);
    void memoryAdd(bool subtract);
    void startPrompt(Prompt p);
    void finishPrompt();
    void cancelPrompt();
    void rebuildSolveFields();
    void openEditor(const QString &name);
    void resizeEditorTable();
    void commitEditor();
    void updateDefineButtons();
    void setBase(int base);
    QString baseString(qint64 v, int base) const;
};
