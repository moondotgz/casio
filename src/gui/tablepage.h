#pragma once
#include "modepage.h"

class QStackedWidget;
class QTableWidget;
class QLabel;

class TablePage : public ModePage {
    Q_OBJECT
public:
    explicit TablePage(AppState *st, QWidget *parent = nullptr);
    QString modeName() const override { return "Table"; }
    QString indicator() const override { return "TABLE"; }
    void execute(bool approx) override;
    void allClear() override;
    void navigate(const QString &dir) override;
    QWidget *editor() const override;
    void activated() override;
    void settingsChanged() override;

private:
    QStackedWidget *m_stack;
    ExprEdit *m_f, *m_g, *m_start, *m_end, *m_step;
    QTableWidget *m_table;
    QLabel *m_error, *m_title;
    NodePtr m_fn, m_gn;
    bool m_updating = false;

    void generate();
    void computeRow(int row);
    QString evalAt(const NodePtr &n, double x);
    double fieldValue(ExprEdit *e);
    QList<ExprEdit *> fields() const;
};
