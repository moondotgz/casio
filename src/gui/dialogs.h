#pragma once
#include "modepage.h"
#include <QDialog>

class QComboBox;
class QSpinBox;
class QCheckBox;

// The MENU screen: a grid of the 12 calculation modes.
class MenuPage : public QWidget {
    Q_OBJECT
public:
    explicit MenuPage(QWidget *parent = nullptr);
    static QStringList modeNames();
    bool handleKey(const QString &keyId);   // 1-9, A(−) B(°'") C(x⁻¹), arrows, =
    void navigate(const QString &dir);
    void setSelected(int i);
signals:
    void modeSelected(int index);   // 0-based
protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseMoveEvent(QMouseEvent *) override;
private:
    int m_sel = 0, m_hover = -1;
    QRectF tileRect(int i) const;
    int tileAt(const QPointF &p) const;
};

class SetupDialog : public QDialog {
    Q_OBJECT
public:
    SetupDialog(Settings &s, QWidget *parent = nullptr);
    void accept() override;
private:
    Settings &m_s;
    QComboBox *m_io, *m_angle, *m_format, *m_frac, *m_complex, *m_decimal;
    QSpinBox *m_digits;
    QCheckBox *m_freq, *m_auto, *m_sep, *m_tableG;
};

class RecallDialog : public QDialog {
    Q_OBJECT
public:
    RecallDialog(const Context &ctx, QWidget *parent = nullptr);
    QString chosen() const { return m_chosen; }
private:
    QString m_chosen;
};
