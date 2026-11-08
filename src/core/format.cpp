#include "format.h"
#include "numeric.h"
#include <cmath>
#include <numeric>

namespace {

const QString MINUS = QStringLiteral("−");

int digitCount(long long v)
{
    v = std::llabs(v);
    int d = 1;
    while (v >= 10) { v /= 10; ++d; }
    return d;
}

QString minusify(QString s)
{
    s.replace('-', QChar(0x2212));
    return s;
}

QString stripZeros(QString m)
{
    if (m.contains('.')) {
        while (m.endsWith('0')) m.chop(1);
        if (m.endsWith('.')) m.chop(1);
    }
    return m;
}

// "1.2345ᴇ−5" with `sig` significant digits
QString sciString(double x, int sig, bool keepZeros)
{
    QString e = QString::number(x, 'e', sig - 1);
    int k = int(e.indexOf('e'));
    QString mant = e.left(k);
    int ex = e.mid(k + 1).toInt();
    if (!keepZeros) mant = stripZeros(mant);
    return minusify(mant) + QChar(0x1D07) + (ex < 0 ? MINUS : QString()) + QString::number(std::abs(ex));
}

// n = k² · b with b square-free
void splitSquare(long long n, long long &k, long long &b)
{
    k = 1;
    b = n;
    for (long long p = 2; p * p <= b; ++p)
        while (b % (p * p) == 0) { b /= p * p; k *= p; }
}

QString fracStr(const QString &num, long long den)
{
    if (den == 1) return num;
    return num + "/" + QString::number(den);
}

} // namespace

QString formatDecimal(double x, const Settings &s)
{
    if (!std::isfinite(x)) return QStringLiteral("ERROR");
    if (s.format == NumFormat::Sci) {
        int sig = s.digits == 0 ? 10 : s.digits;
        if (x == 0) {
            QString m = "0";
            if (sig > 1) m += "." + QString(sig - 1, '0');
            return m + QChar(0x1D07) + "0";
        }
        return sciString(x, sig, true);
    }
    if (x == 0) return s.format == NumFormat::Fix && s.digits > 0 ? "0." + QString(s.digits, '0') : QStringLiteral("0");
    // round to 10 significant digits first
    QString e = QString::number(x, 'e', 9);
    double xr = e.toDouble();
    int ex = e.mid(e.indexOf('e') + 1).toInt();
    if (s.format == NumFormat::Fix) {
        if (std::abs(xr) >= 1e10) return sciString(x, s.digits + 1, true);
        QString r = QString::number(xr, 'f', s.digits);
        if (r.startsWith('-') && r.mid(1).toDouble() == 0) r = r.mid(1);
        return minusify(r);
    }
    double lo = s.format == NumFormat::Norm1 ? 1e-2 : 1e-9;
    if (std::abs(xr) >= 1e10 || std::abs(xr) < lo) return sciString(x, 10, false);
    int decimals = std::max(0, 9 - ex);
    return minusify(stripZeros(QString::number(xr, 'f', decimals)));
}

QString formatEng(double x, const Settings &, int shift)
{
    if (x == 0) return QStringLiteral("0");
    QString e = QString::number(x, 'e', 9);
    int ex = e.mid(e.indexOf('e') + 1).toInt();
    int e3 = int(std::floor(ex / 3.0)) * 3 - 3 * shift;
    double mant = e.left(e.indexOf('e')).toDouble() * std::pow(10.0, ex - e3);
    int intDigits = std::max(1, int(std::floor(std::log10(std::abs(mant)))) + 1);
    int decimals = std::clamp(10 - intDigits, 0, 15);
    QString m = stripZeros(QString::number(mant, 'f', decimals));
    return minusify(m) + QChar(0x1D07) + (e3 < 0 ? MINUS : QString()) + QString::number(std::abs(e3));
}

QString formatDMS(double x)
{
    if (!std::isfinite(x) || std::abs(x) >= 1e10) return formatDecimal(x, Settings{});
    bool neg = x < 0;
    double a = std::abs(x);
    double d = std::floor(a);
    double mf = (a - d) * 60;
    double m = std::floor(mf + 1e-9);
    double s = (mf - m) * 60;
    int dd = digitCount(static_cast<long long>(d));
    int dec = std::clamp(8 - dd, 0, 6);
    double p = std::pow(10.0, dec);
    s = std::round(s * p) / p;
    if (s < 0) s = 0;
    if (s >= 60) { s -= 60; m += 1; }
    if (m >= 60) { m -= 60; d += 1; }
    QString ss = stripZeros(QString::number(s, 'f', dec));
    return (neg ? MINUS : QString()) + QString::number(static_cast<long long>(d)) + QChar(0x00B0) +
           QString::number(static_cast<long long>(m)) + "'" + ss + "\"";
}

QString exactReal(double x, FracFormat frac)
{
    if (!std::isfinite(x)) return {};
    if (x == 0) return QStringLiteral("0");
    const bool neg = x < 0;
    const double ax = std::abs(x);
    if (ax >= 1e10 || ax < 1e-10) return {};
    auto sign = [&](const QString &body) {
        if (!neg) return body;
        if (body.contains('/') || body.contains(QChar(0x231F))) return MINUS + "{" + body + "}";
        return MINUS + body;
    };

    // integer
    if (ax == std::floor(ax)) return sign(QString::number(static_cast<long long>(ax)));

    long long p, q;
    // rational p/q, at most 10 digits in total (as on the calculator)
    if (num::smallRational(ax, p, q, 1000000000LL, 1e-13)) {
        if (frac == FracFormat::Mixed && p > q) {
            long long w = p / q, r = p % q;
            if (digitCount(w) + digitCount(r) + digitCount(q) <= 10)
                return sign(QString::number(w) + QChar(0x231F) + QString::number(r) + QChar(0x231F) + QString::number(q));
        } else if (digitCount(p) + digitCount(q) <= 10) {
            return sign(fracStr(QString::number(p), q));
        }
    }

    // a√b / c   (x² rational)
    {
        double sq = ax * ax;
        long long n, d;
        if (num::smallRational(sq, n, d, 10000, 2e-13) && n < 100000000LL) {
            long long k, b;
            splitSquare(n * d, k, b);
            if (b > 1) {
                long long den = d, a = k;
                long long g = std::gcd(a, den);
                a /= g;
                den /= g;
                double v = double(a) * std::sqrt(double(b)) / double(den);
                if (std::abs(v - ax) <= 1e-12 * ax && digitCount(a) + digitCount(b) + digitCount(den) <= 10) {
                    QString body = (a == 1 ? QString() : QString::number(a)) + QChar(0x221A) + "(" + QString::number(b) + ")";
                    return sign(fracStr(body, den));
                }
            }
        }
    }

    // p π / q
    {
        double y = ax / M_PI;
        if (num::smallRational(y, p, q, 1000, 1e-13) && p < 100000 && p > 0) {
            QString body = (p == 1 ? QString() : QString::number(p)) + QChar(0x03C0);
            return sign(fracStr(body, q));
        }
    }

    // (a + b√c) / d
    {
        static const int sqf[] = {2, 3, 5, 6, 7, 10, 11, 13, 14, 15, 17, 19, 21, 22, 23, 26, 29, 30, 31, 33, 34, 35, 37, 38, 39, 41, 42, 43, 46, 47};
        for (long long d = 1; d <= 50; ++d) {
            double y = x * double(d);
            for (int c : sqf) {
                double rc = std::sqrt(double(c));
                for (long long b = -50; b <= 50; ++b) {
                    if (b == 0) continue;
                    double a = y - double(b) * rc;
                    double ar = std::round(a);
                    if (ar == 0 || std::abs(ar) > 10000) continue;
                    if (std::abs(a - ar) > 2e-12 * std::max(1.0, std::abs(y))) continue;
                    long long ai = static_cast<long long>(ar);
                    if (std::gcd(std::gcd(std::llabs(ai), std::llabs(b)), d) != 1) continue;
                    QString body = QString::number(ai).replace('-', MINUS) + (b > 0 ? "+" : MINUS) +
                                   (std::llabs(b) == 1 ? QString() : QString::number(std::llabs(b))) +
                                   QChar(0x221A) + "(" + QString::number(c) + ")";
                    if (d == 1) return body;
                    return "{" + body + "}/" + QString::number(d);
                }
            }
        }
    }
    return {};
}

QString formatReal(double x, const Settings &s, const DisplayOpts &o)
{
    if (o.dms) return formatDMS(x);
    if (o.eng) return formatEng(x, s, o.engShift);
    if (!o.approx && s.exactOutput()) {
        FracFormat f = s.frac;
        if (o.swapFraction) f = (f == FracFormat::Mixed) ? FracFormat::Improper : FracFormat::Mixed;
        QString e = exactReal(x, f);
        if (!e.isEmpty()) return e;
    }
    return formatDecimal(x, s);
}

QString formatComplex(cplx z, const Settings &s, const DisplayOpts &o)
{
    bool polar = o.form == ForcedForm::Polar || (o.form == ForcedForm::None && s.complexForm == ComplexForm::Polar);
    if (polar && z != cplx(0, 0)) {
        double r = std::abs(z);
        double th = std::arg(z);
        if (s.angle == AngleUnit::Deg) th = th * 180.0 / M_PI;
        else if (s.angle == AngleUnit::Gra) th = th * 200.0 / M_PI;
        DisplayOpts po = o;
        po.dms = false;
        return formatReal(r, s, po) + QChar(0x2220) + "{" + formatReal(th, s, po) + "}";
    }
    double re = z.real(), im = z.imag();
    if (im == 0) return formatReal(re, s, o);
    DisplayOpts io = o;
    io.dms = false;
    QString imStr = std::abs(im) == 1 ? QStringLiteral("i") : "{" + formatReal(std::abs(im), s, io) + "}i";
    if (re == 0) return (im < 0 ? MINUS : QString()) + imStr;
    return formatReal(re, s, io) + (im < 0 ? MINUS : QStringLiteral("+")) + imStr;
}

QString formatBaseN(qint64 v, int base)
{
    if (base == 10) return minusify(QString::number(v));
    return QString::number(quint32(qint32(v)), base).toUpper();
}

QString toLinearText(const QString &syntax)
{
    QString s = syntax;
    s.remove('{');
    s.remove('}');
    s.replace(QChar(0x1D07), QStringLiteral("×10^"));
    return s;
}

QString toDisplayText(const QString &syntax)
{
    QString s = syntax;
    s.remove('{');
    s.remove('}');
    int e = int(s.indexOf(QChar(0x1D07)));
    if (e < 0) return s;
    static const QString sup = QStringLiteral("\u2070\u00B9\u00B2\u00B3\u2074\u2075\u2076\u2077\u2078\u2079");
    QString ex;
    int i = e + 1;
    if (i < s.size() && (s[i] == QChar(0x2212) || s[i] == '-')) { ex += QChar(0x207B); ++i; }
    while (i < s.size() && s[i].isDigit()) ex += sup[s[i++].digitValue()];
    return s.left(e) + QStringLiteral("\u00D710") + ex + toDisplayText(s.mid(i));
}

bool hasExactForm(const Value &v, const Settings &s)
{
    DisplayOpts a;
    a.approx = true;
    if (v.isScalar()) return formatComplex(v.z, s) != formatComplex(v.z, s, a);
    for (const auto &e : v.m)
        if (formatComplex(e, s) != formatComplex(e, s, a)) return true;
    return false;
}
