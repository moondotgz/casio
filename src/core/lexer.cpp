#include "lexer.h"
#include "error.h"
#include <QHash>
#include <QRegularExpression>
#include <algorithm>
#include <cmath>

namespace {

struct Keyword {
    QString name;
    TokType type;
};

const QVector<Keyword> &keywordTable()
{
    static const QVector<Keyword> table = [] {
        QVector<Keyword> t;
        auto f = [&](const QString &n) { t.push_back({n, TokType::Func}); };
        auto v = [&](const QString &n) { t.push_back({n, TokType::Var}); };
        auto in = [&](const QString &n) { t.push_back({n, TokType::Infix}); };
        auto pf = [&](const QString &n) { t.push_back({n, TokType::Postfix}); };
        const QString inv = sym::Inv;
        for (const char *n : {"sin", "cos", "tan", "sinh", "cosh", "tanh"}) {
            f(QString::fromLatin1(n));
            f(QString::fromLatin1(n) + inv);
        }
        for (const char *n : {"asin", "acos", "atan", "asinh", "acosh", "atanh", "log", "ln", "root",
                              "Abs", "Pol", "Rec", "Int", "Intg", "Rnd", "RanInt#", "RanInt", "GCD", "LCM",
                              "d/dx", "Conjg", "Arg", "ReP", "ImP", "Det", "Trn", "Identity", "Ref", "Rref",
                              "DotP", "CrossP", "UnitV", "Angle", "Not", "Neg", "Min", "Max", "Mean", "Sum",
                              "mixed", "exp"})
            f(QString::fromLatin1(n));
        f(sym::Sqrt);
        f(sym::Cbrt);
        f(sym::Integral);
        f(sym::Sigma);
        f(sym::Prod);
        f(QStringLiteral("Ran#"));
        for (const char *n : {"A", "B", "C", "D", "E", "F", "M", "x", "y", "Ans", "PreAns", "MatA", "MatB",
                              "MatC", "MatD", "MatAns", "VctA", "VctB", "VctC", "VctD", "VctAns", "e", "i"})
            v(QString::fromLatin1(n));
        v(sym::Pi);
        in(QStringLiteral("nPr"));
        in(QStringLiteral("nCr"));
        in(QStringLiteral("and"));
        in(QStringLiteral("or"));
        in(QStringLiteral("xor"));
        in(QStringLiteral("xnor"));
        in(sym::DivR);
        pf(sym::ToPolar);
        pf(sym::ToRect);
        return t;
    }();
    return table;
}

bool isDigitIn(QChar c, int base)
{
    int d;
    if (c >= '0' && c <= '9') d = c.unicode() - '0';
    else if (c >= 'A' && c <= 'F') d = c.unicode() - 'A' + 10;
    else return false;
    return d < base;
}

int digitVal(QChar c)
{
    if (c >= '0' && c <= '9') return c.unicode() - '0';
    return c.unicode() - 'A' + 10;
}

bool isMinusChar(QChar c) { return c == '-' || c == QChar(0x2212); }

} // namespace

const QStringList &allKeywords()
{
    static const QStringList list = [] {
        QStringList l;
        for (const auto &k : keywordTable())
            if (k.name.size() > 1) l << k.name;
        l << QStringLiteral("x̂") << QStringLiteral("ŷ") << sym::Inv;
        std::sort(l.begin(), l.end(), [](const QString &a, const QString &b) { return a.size() > b.size(); });
        return l;
    }();
    return list;
}

QVector<Token> tokenize(const QString &input, const LexOptions &opt)
{
    // canonical decomposition so precomposed ŷ / ȳ match the y + combining-mark keywords
    const QString src = input.normalized(QString::NormalizationForm_D);
    QVector<Token> out;
    const int n = src.size();
    int i = 0;

    // Build the active keyword list (longest match first)
    QVector<Keyword> kws;
    if (opt.base) {
        for (const char *k : {"and", "or", "xor", "xnor", "Not", "Neg", "Ans", "PreAns"}) {
            QString s = QString::fromLatin1(k);
            TokType t = (s == "Not" || s == "Neg") ? TokType::Func
                        : (s == "Ans" || s == "PreAns") ? TokType::Var : TokType::Infix;
            kws.push_back({s, t});
        }
    } else {
        kws = keywordTable();
        for (const auto &s : opt.extraVars) kws.push_back({s, TokType::Var});
        for (const auto &s : opt.extraFuncs) kws.push_back({s, TokType::Func});
    }
    std::stable_sort(kws.begin(), kws.end(), [](const Keyword &a, const Keyword &b) { return a.name.size() > b.name.size(); });

    auto push = [&](TokType t, const QString &text, int pos, int len, double num = 0) {
        Token tk;
        tk.type = t;
        tk.text = text;
        tk.pos = pos;
        tk.len = len;
        tk.num = num;
        out.push_back(tk);
    };

    while (i < n) {
        QChar c = src[i];
        if (c.isSpace()) { ++i; continue; }
        const int start = i;

        // ---------- Base-N numbers ----------
        if (opt.base) {
            int base = opt.base;
            int j = i;
            if ((c == 'd' || c == 'h' || c == 'b' || c == 'o') && i + 1 < n) {
                int pb = c == 'd' ? 10 : c == 'h' ? 16 : c == 'b' ? 2 : 8;
                if (isDigitIn(src[i + 1], pb)) { base = pb; j = i + 1; }
            }
            if (j < n && isDigitIn(src[j], base)) {
                unsigned long long v = 0;
                while (j < n && isDigitIn(src[j], base)) {
                    v = v * base + digitVal(src[j]);
                    if (v > 0xFFFFFFFFull) throw CalcError(ErrKind::Math, start);
                    ++j;
                }
                double val;
                if (base == 10) {
                    if (v > 0x7FFFFFFFull) throw CalcError(ErrKind::Math, start);
                    val = double(v);
                } else {
                    val = double(qint32(quint32(v)));   // two's complement
                }
                push(TokType::Num, src.mid(start, j - start), start, j - start, val);
                i = j;
                continue;
            }
        }

        // ---------- Spreadsheet cell references ----------
        if (opt.cellRefs && ((c >= 'A' && c <= 'E') || c == '$')) {
            static const QRegularExpression re(QStringLiteral("^\\$?([A-E])\\$?([0-9]{1,2})(?::\\$?([A-E])\\$?([0-9]{1,2}))?"));
            auto m = re.matchView(QStringView(src).mid(i));
            if (m.hasMatch()) {
                int len = int(m.capturedLength(0));
                QString t = src.mid(i, len);
                t.remove('$');
                push(m.capturedLength(3) ? TokType::Range : TokType::Cell, t, start, len);
                i += len;
                continue;
            }
        }

        // ---------- Decimal numbers (with ᴇ exponent and D°M'S" literals) ----------
        if (!opt.base && (c.isDigit() || (c == '.' && i + 1 < n && src[i + 1].isDigit()))) {
            int j = i;
            while (j < n && src[j].isDigit()) ++j;
            if (j < n && src[j] == '.') {
                ++j;
                while (j < n && src[j].isDigit()) ++j;
            }
            QString mant = src.mid(i, j - i);
            double val = mant.toDouble();
            // exponent: ᴇ or E followed by optional sign and digits
            if (j < n && (src[j] == QChar(0x1D07) || src[j] == 'E')) {
                int k = j + 1;
                bool neg = false;
                if (k < n && (isMinusChar(src[k]) || src[k] == '+')) { neg = isMinusChar(src[k]); ++k; }
                if (k < n && src[k].isDigit()) {
                    int e0 = k;
                    while (k < n && src[k].isDigit()) ++k;
                    int ex = src.mid(e0, k - e0).toInt();
                    val = (mant + "e" + (neg ? "-" : "") + QString::number(ex)).toDouble();
                    j = k;
                } else if (src[j] == QChar(0x1D07)) {
                    throw CalcError(ErrKind::Syntax, j);
                }
            }
            // sexagesimal  d°m'  /  d°m's"   (also d°m°s° Casio style)
            if (j < n && src[j] == QChar(0x00B0) && j + 1 < n && (src[j + 1].isDigit() || src[j + 1] == '.')) {
                int k = j + 1;
                auto readNum = [&](int &p) {
                    int s0 = p;
                    while (p < n && (src[p].isDigit() || src[p] == '.')) ++p;
                    return src.mid(s0, p - s0).toDouble();
                };
                double mins = readNum(k);
                double secs = 0;
                if (k < n && (src[k] == '\'' || src[k] == QChar(0x00B0) || src[k] == QChar(0x2032))) {
                    ++k;
                    if (k < n && (src[k].isDigit() || src[k] == '.')) {
                        secs = readNum(k);
                        if (k < n && (src[k] == '"' || src[k] == QChar(0x00B0) || src[k] == QChar(0x2033))) ++k;
                    }
                }
                val = val + mins / 60.0 + secs / 3600.0;
                j = k;
            }
            push(TokType::Num, src.mid(i, j - i), start, j - i, val);
            i = j;
            continue;
        }

        // ---------- Constants and conversions ‹...› ----------
        if (c == QChar(0x2039)) {
            int j = src.indexOf(QChar(0x203A), i + 1);
            if (j < 0) throw CalcError(ErrKind::Syntax, i);
            QString body = src.mid(i + 1, j - i - 1);
            push(body.contains(QChar(0x25B6)) ? TokType::Conv : TokType::Const, body, start, j - i + 1);
            i = j + 1;
            continue;
        }

        // ---------- Keywords (longest match) ----------
        bool matched = false;
        for (const auto &k : kws) {
            if (src.mid(i, k.name.size()) == k.name) {
                // a lone letter keyword must not swallow part of a longer identifier we don't know about
                push(k.type, k.name, start, int(k.name.size()));
                i += int(k.name.size());
                matched = true;
                break;
            }
        }
        if (matched) continue;

        // ---------- Single-character operators ----------
        switch (c.unicode()) {
        case '+': push(TokType::Op, "+", start, 1); ++i; continue;
        case '-': case 0x2212: push(TokType::Op, sym::Minus, start, 1); ++i; continue;
        case '*': case 0x00D7: case 0x00B7: push(TokType::Op, sym::Times, start, 1); ++i; continue;
        case 0x00F7: push(TokType::Op, sym::Divide, start, 1); ++i; continue;
        case '/': push(TokType::Op, "/", start, 1); ++i; continue;
        case '^': push(TokType::Op, "^", start, 1); ++i; continue;
        case 0x2220: push(TokType::Op, sym::Angle, start, 1); ++i; continue;
        case 0x2192: push(TokType::Op, sym::Arrow, start, 1); ++i; continue;
        case '=': push(TokType::Op, "=", start, 1); ++i; continue;
        case '<':
            if (i + 1 < n && src[i + 1] == '=') { push(TokType::Op, QStringLiteral("≤"), start, 2); i += 2; continue; }
            push(TokType::Op, "<", start, 1); ++i; continue;
        case '>':
            if (i + 1 < n && src[i + 1] == '=') { push(TokType::Op, QStringLiteral("≥"), start, 2); i += 2; continue; }
            push(TokType::Op, ">", start, 1); ++i; continue;
        case 0x2264: case 0x2265: push(TokType::Op, QString(c), start, 1); ++i; continue;
        case 0x231F: push(TokType::Op, sym::Mixed, start, 1); ++i; continue;
        case '(': push(TokType::LParen, "(", start, 1); ++i; continue;
        case ')': push(TokType::RParen, ")", start, 1); ++i; continue;
        case '{': push(TokType::LGroup, "{", start, 1); ++i; continue;
        case '}': push(TokType::RGroup, "}", start, 1); ++i; continue;
        case ',': push(TokType::Comma, ",", start, 1); ++i; continue;
        case ':': push(TokType::Colon, ":", start, 1); ++i; continue;
        case '!': case '%': case 0x00B2: case 0x00B3: case 0x00B0: case 0x02B3: case 0x1D4D:
            push(TokType::Postfix, QString(c), start, 1); ++i; continue;
        case 0x207B:
            if (i + 1 < n && src[i + 1] == QChar(0x00B9)) { push(TokType::Postfix, sym::Inv, start, 2); i += 2; continue; }
            break;
        default: break;
        }
        throw CalcError(ErrKind::Syntax, i);
    }
    Token end;
    end.type = TokType::End;
    end.pos = n;
    out.push_back(end);
    return out;
}
