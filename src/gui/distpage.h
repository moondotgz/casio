#pragma once
#include "modepage.h"

class QComboBox;
class QRadioButton;
class QFormLayout;
class QTableWidget;
class QLabel;

class DistPage : public ModePage {
    Q_OBJECT
public:
    explicit DistPage(AppState *st, QWidget *parent = nullptr);
    QString modeName() const override { return "Distribution"; }
    QString indicator() const override { return "DIST"; }
    void execute(bool approx) override;
    void allClear() override;
    void navigate(const QString &dir) override;
    QWidget *editor() const override;
    void activated() override;
    QList<MenuItem> optnMenu() const override;
    void command(const QString &cmd) override;

private:
    enum Type { NormPD, NormCD, InvNorm, BinPD, BinCD, PoiPD, PoiCD };
    QComboBox *m_type;
    QRadioButton *m_variable, *m_list;
    QComboBox *m_tail;
    QFormLayout *m_form;
    QList<QPair<QString, ExprEdit *>> m_fields;
    MathView *m_result;
    QTableWidget *m_table;
    QLabel *m_error;
    QLabel *m_tailLabel;

    Type type() const;
    void rebuildForm();
    double field(const QString &name);
};
