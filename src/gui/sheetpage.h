#pragma once
#include "modepage.h"
#include <QMap>
#include <QSet>

class QTableWidget;
class QLabel;
class QStackedWidget;

class SheetPage : public ModePage {
    Q_OBJECT
public:
    explicit SheetPage(AppState *st, QWidget *parent = nullptr);
    QString modeName() const override { return "Spreadsheet"; }
    QString indicator() const override { return "SHEET"; }
    void execute(bool approx) override;
    void allClear() override;
    void navigate(const QString &dir) override;
    void command(const QString &cmd) override;
    QList<MenuItem> optnMenu() const override;
    QWidget *editor() const override;
    void activated() override;
    void settingsChanged() override { refreshDisplay(); }

    static constexpr int Rows = 45, Cols = 5;

private:
    QStackedWidget *m_stack;
    QTableWidget *m_table;
    ExprEdit *m_edit;
    QLabel *m_cellLabel, *m_error;
    QMap<QString, QString> m_raw;
    QMap<QString, Value> m_val;
    QMap<QString, ErrKind> m_err;
    // fill panel
    QLabel *m_fillTitle;
    ExprEdit *m_fillExpr, *m_fillRange;
    bool m_fillFormula = true;

    static QString cellName(int r, int c);
    static bool parseCell(const QString &s, int &r, int &c);
    QString currentCell() const;
    void loadCurrent();
    void commit(const QString &cell, const QString &text);
    void recalc();
    Value evalCell(const QString &name);
    Value evalFormula(const QString &expr);
    QSet<QString> m_visiting;
    int m_depth = 0;
    void refreshDisplay();
    void startFill(bool formula);
    void finishFill();
    static QString shiftRefs(const QString &formula, int dr, int dc, bool &ok);
};
