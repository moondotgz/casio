#pragma once
#include "modepage.h"

class QComboBox;
class QStackedWidget;
class QTableWidget;
class QLabel;

// Shared helper: coefficient grid backed by ExprEdit cells.
class CoefficientPage : public ModePage {
    Q_OBJECT
public:
    using ModePage::ModePage;
    void insertText(const QString &text, bool startsNew) override;
    void navigate(const QString &dir) override;
protected:
    QTableWidget *m_coef = nullptr;
    QTableWidget *makeGrid(QWidget *parent);
    void setGrid(int rows, const QStringList &headers);
    std::vector<std::vector<double>> readGrid();   // throws CalcError
};

class EquationPage : public CoefficientPage {
    Q_OBJECT
public:
    explicit EquationPage(AppState *st, QWidget *parent = nullptr);
    QString modeName() const override { return "Equation/Func"; }
    QString indicator() const override { return "EQN"; }
    void execute(bool approx) override;
    void allClear() override;
    void activated() override;
    void settingsChanged() override;
    void command(const QString &cmd) override;
    QList<MenuItem> optnMenu() const override;

private:
    QComboBox *m_type;
    QStackedWidget *m_stack;
    MathView *m_result;
    QLabel *m_error, *m_resTitle;
    bool m_approx = false;
    void setupGrid();
    void solve();
};
