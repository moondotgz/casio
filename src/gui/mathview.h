#pragma once
#include "core/format.h"
#include "core/lexer.h"
#include "core/settings.h"
#include "core/value.h"
#include <QColor>
#include <QPainter>
#include <QWidget>
#include <memory>
#include <vector>

// ------------------------------------------------------------------ layout boxes

class Box {
public:
    qreal w = 0, asc = 0, desc = 0;
    virtual ~Box() = default;
    virtual void draw(QPainter &p, qreal x, qreal y) const = 0;   // y = baseline
    qreal height() const { return asc + desc; }
};
using BoxPtr = std::shared_ptr<Box>;

struct MathStyle {
    QString family;
    qreal size = 18;
    QColor color = QColor(20, 22, 20);
    bool natural = true;
    const Settings *settings = nullptr;
    MathStyle();
};

BoxPtr layoutText(const QString &t, const MathStyle &st, qreal scale = 1.0, bool italic = false, bool bold = false);
BoxPtr layoutSyntax(const QString &syntax, const MathStyle &st, const LexOptions &lex = {});
BoxPtr layoutValue(const Value &v, const Settings &s, const DisplayOpts &o, const MathStyle &st);
BoxPtr layoutRow(std::vector<BoxPtr> items, qreal gap = 0);
BoxPtr layoutLines(std::vector<BoxPtr> lines, bool rightAlign, qreal gap = 4);

// "label = value" line, e.g. for x₁=…, r=…, θ=…
BoxPtr layoutLabeled(const QString &labelSyntax, const BoxPtr &value, const MathStyle &st);

// ------------------------------------------------------------------ widget

class MathView : public QWidget {
    Q_OBJECT
public:
    explicit MathView(QWidget *parent = nullptr);

    void setBox(BoxPtr b);
    void clear() { setBox(nullptr); }
    BoxPtr box() const { return m_box; }
    void setAlignment(Qt::Alignment a) { m_align = a; update(); }
    void setFixedLines(int n) { m_minLines = n; updateGeometry(); }
    void setInverted(bool inv) { m_inverted = inv; update(); }
    MathStyle &style() { return m_style; }

    QSize sizeHint() const override;
    QSize minimumSizeHint() const override;

signals:
    void clicked();

protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;

private:
    BoxPtr m_box;
    Qt::Alignment m_align = Qt::AlignLeft | Qt::AlignVCenter;
    MathStyle m_style;
    int m_minLines = 1;
    bool m_inverted = false;
};
