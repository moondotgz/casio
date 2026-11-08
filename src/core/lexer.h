#pragma once
#include <QString>
#include <QStringList>
#include <QVector>

enum class TokType {
    Num,      // numeric literal (num holds the value)
    Var,      // variable / named constant (A..F, M, x, y, Ans, π, e, i, MatA ...)
    Func,     // function name; usually followed by '('
    Infix,    // word operators: nPr nCr and or xor xnor ÷R
    Postfix,  // ! ² ³ ⁻¹ % ° ʳ ᵍ ▶r∠θ ▶a+bi
    Op,       // + - × ÷ / ^ ∠ → = < > ≤ ≥ ⌟
    LParen, RParen, LGroup, RGroup, Comma, Colon,
    Const,    // ‹id›  scientific constant
    Conv,     // ‹a▶b› unit conversion (postfix)
    Cell,     // spreadsheet cell reference  A1 .. E45
    Range,    // spreadsheet range  A1:B5
    End
};

struct Token {
    TokType type = TokType::End;
    QString text;
    double num = 0;
    int pos = 0;
    int len = 0;
};

struct LexOptions {
    int base = 0;              // 0 = normal, otherwise Base-N mode (2, 8, 10, 16)
    bool cellRefs = false;     // spreadsheet mode
    QStringList extraVars;     // e.g. statistics variables
    QStringList extraFuncs;    // e.g. x̂, ŷ
};

QVector<Token> tokenize(const QString &src, const LexOptions &opt = {});

// All multi-character tokens recognized by the lexer, longest first. Used for smart delete.
const QStringList &allKeywords();

namespace sym {
inline const QString Minus = QStringLiteral("−");     // −
inline const QString Times = QStringLiteral("×");     // ×
inline const QString Divide = QStringLiteral("÷");    // ÷
inline const QString Sqrt = QStringLiteral("√");      // √
inline const QString Cbrt = QStringLiteral("∛");      // ∛
inline const QString Pi = QStringLiteral("π");        // π
inline const QString Angle = QStringLiteral("∠");     // ∠
inline const QString Arrow = QStringLiteral("→");     // →
inline const QString Exp10 = QStringLiteral("ᴇ");     // ᴇ  (×10^)
inline const QString Sq = QStringLiteral("²");        // ²
inline const QString Cube = QStringLiteral("³");      // ³
inline const QString Inv = QStringLiteral("⁻¹"); // ⁻¹
inline const QString Deg = QStringLiteral("°");       // °
inline const QString RadU = QStringLiteral("ʳ");      // ʳ
inline const QString GraU = QStringLiteral("ᵍ");      // ᵍ
inline const QString Integral = QStringLiteral("∫");  // ∫
inline const QString Sigma = QStringLiteral("Σ");     // Σ
inline const QString Prod = QStringLiteral("Π");      // Π
inline const QString Mixed = QStringLiteral("⌟");     // ⌟
inline const QString ToPolar = QStringLiteral("▶r∠θ");  // ▶r∠θ
inline const QString ToRect = QStringLiteral("▶a+bi");            // ▶a+bi
inline const QString DivR = QStringLiteral("÷R");                 // ÷R
inline const QString ConstOpen = QStringLiteral("‹");  // ‹
inline const QString ConstClose = QStringLiteral("›"); // ›
}
