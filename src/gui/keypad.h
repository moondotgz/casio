#pragma once
#include <QWidget>
#include <QVector>

enum class KeyLayer { Main, Shift, Alpha };
enum class KeyStyle { Function, Number, Operator, Accent, Modifier, Nav };

// Actions: "i:<text>" inserts text (§ marks the cursor position), "c:<CMD>" is a command.
struct KeyDef {
    QString id;
    QString label, shiftLabel, alphaLabel;
    QString action, shiftAction, alphaAction;
    KeyStyle style = KeyStyle::Function;

    QString actionFor(KeyLayer l) const
    {
        switch (l) {
        case KeyLayer::Shift: return shiftAction;
        case KeyLayer::Alpha: return alphaAction;
        default: return action;
        }
    }
};

class KeyButton : public QWidget {
    Q_OBJECT
public:
    KeyButton(const KeyDef &def, QWidget *parent = nullptr);
    const KeyDef &def() const { return m_def; }
    void setLayer(KeyLayer l) { m_layer = l; update(); }
    void setLatched(bool b) { m_latched = b; update(); }
    QSize sizeHint() const override;

signals:
    void pressed(const KeyDef &def);

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
    void enterEvent(QEnterEvent *) override;
    void leaveEvent(QEvent *) override;

private:
    KeyDef m_def;
    KeyLayer m_layer = KeyLayer::Main;
    bool m_down = false, m_hover = false, m_latched = false;
};

class NavPad : public QWidget {   // ◀ ▲ ▼ ▶ replay pad
    Q_OBJECT
public:
    explicit NavPad(QWidget *parent = nullptr);
    QSize sizeHint() const override { return {120, 96}; }
signals:
    void nav(const QString &cmd);   // LEFT RIGHT UP DOWN
protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void mouseReleaseEvent(QMouseEvent *) override;
private:
    QString hit(const QPointF &p) const;
    QString m_down;
};

class Keypad : public QWidget {
    Q_OBJECT
public:
    explicit Keypad(QWidget *parent = nullptr);
    void setLayer(KeyLayer l);
    void setShiftLatched(bool b);
    void setAlphaLatched(bool b);
    const QVector<KeyDef> &keys() const { return m_defs; }

signals:
    void keyPressed(const KeyDef &def);
    void nav(const QString &cmd);

private:
    QVector<KeyDef> m_defs;
    QVector<KeyButton *> m_buttons;
    KeyButton *m_shift = nullptr, *m_alpha = nullptr;
};
