#pragma once
#include "equationpage.h"

class InequalityPage : public CoefficientPage {
    Q_OBJECT
public:
    explicit InequalityPage(AppState *st, QWidget *parent = nullptr);
    QString modeName() const override { return "Inequality"; }
    QString indicator() const override { return "INEQ"; }
    void execute(bool approx) override;
    void allClear() override;
    void activated() override;
    void settingsChanged() override;
    void command(const QString &cmd) override;
    QList<MenuItem> optnMenu() const override;

private:
    QComboBox *m_degree, *m_rel;
    QStackedWidget *m_stack;
    MathView *m_result;
    QLabel *m_error, *m_resTitle;
    bool m_approx = false;
    void setupGrid();
    void solve();
};

class RatioPage : public ModePage {
    Q_OBJECT
public:
    explicit RatioPage(AppState *st, QWidget *parent = nullptr);
    QString modeName() const override { return "Ratio"; }
    QString indicator() const override { return "RATIO"; }
    void execute(bool approx) override;
    void navigate(const QString &dir) override;
    QWidget *editor() const override;
    void activated() override;
    void command(const QString &cmd) override;
    QList<MenuItem> optnMenu() const override
    {
        return {{"Select Type", {}, {{"A : B = X : D", "c:TYPE:0"}, {"A : B = C : X", "c:TYPE:1"}}}};
    }

private:
    QComboBox *m_type;
    ExprEdit *m_a, *m_b, *m_c;
    QLabel *m_cLabel, *m_error;
    MathView *m_eq, *m_result;
    bool m_approx = false;
    void updateEquation();
};
