#pragma once
#include <QFont>
#include <QPainter>
#include <QString>
#include <QStyledItemDelegate>

// Tiny markup for labels that must not depend on the font having superscript glyphs:
//   ^{..}  superscript      _{..}  subscript      []  empty template box
// unicodeToMarkup() converts ² ³ ⁻¹ ⁿ ˣ ₐ ₚ ☐ ∛ ... into this markup, so existing strings work.

QString unicodeToMarkup(const QString &s);
qreal richWidth(const QString &markup, const QFont &f);
// Draws using the painter's current pen colour.
void drawRich(QPainter &p, const QRectF &rect, const QString &markup, const QFont &f, Qt::Alignment align);
// Draws, shrinking the font (down to minPx) until it fits rect's width.
void drawRichFit(QPainter &p, const QRectF &rect, const QString &markup, QFont f, Qt::Alignment align, int minPx = 6);

// Replaces only the characters the font cannot show with ASCII spellings (x² → x^2, mₚ → m_p).
QString fontSafe(const QString &s, const QFont &f);

// Applies fontSafe() to every combo-box item and plain-text label below root.
void makeFontSafe(QWidget *root);

// Item delegate that paints cell text with drawRich (superscript exponents etc.).
class RichDelegate : public QStyledItemDelegate {
    Q_OBJECT
public:
    using QStyledItemDelegate::QStyledItemDelegate;
    void paint(QPainter *p, const QStyleOptionViewItem &opt, const QModelIndex &idx) const override;
};
