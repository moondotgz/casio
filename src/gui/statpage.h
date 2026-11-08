#pragma once
#include "core/stats.h"
#include "modepage.h"
#include <optional>

class QComboBox;
class QCheckBox;
class QTabWidget;
class QTableWidget;
class QLabel;

class StatPage : public ModePage {
    Q_OBJECT
public:
    explicit StatPage(AppState *st, QWidget *parent = nullptr);
    QString modeName() const override { return "Statistics"; }
    QString indicator() const override { return "STAT"; }
    void execute(bool approx) override;
    void allClear() override;
    void command(const QString &cmd) override;
    QList<MenuItem> optnMenu() const override;
    QWidget *editor() const override;
    void insertText(const QString &text, bool startsNew) override;
    void navigate(const QString &dir) override;
    void activated() override;
    void settingsChanged() override;
    void prepareContext() override;

private:
    QComboBox *m_type;
    QCheckBox *m_freq;
    QTabWidget *m_tabs;
    QTableWidget *m_data;
    QTableWidget *m_results;
    QLabel *m_resultTitle;
    ExprEdit *m_calc;
    MathView *m_calcResult;
    QLabel *m_calcError;
    std::optional<StatResult> m_stat;

    RegType type() const;
    void setupColumns();
    void cellChanged(int row, int col);
    StatData collect();
    bool compute();
    void showResults(bool regression);
};
