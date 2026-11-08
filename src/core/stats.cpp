#include "stats.h"
#include "error.h"
#include <algorithm>
#include <cmath>

QString regTypeName(RegType t)
{
    switch (t) {
    case RegType::OneVar: return QStringLiteral("1-Variable");
    case RegType::Linear: return QStringLiteral("y=a+bx");
    case RegType::Quadratic: return QStringLiteral("y=a+bx+cx²");
    case RegType::Log: return QStringLiteral("y=a+b·ln(x)");
    case RegType::ExpE: return QStringLiteral("y=a·e^(bx)");
    case RegType::ExpAB: return QStringLiteral("y=a·b^x");
    case RegType::Power: return QStringLiteral("y=a·x^b");
    case RegType::Inverse: return QStringLiteral("y=a+b/x");
    }
    return {};
}

bool regIsPaired(RegType t) { return t != RegType::OneVar; }

QStringList statVariableNames()
{
    return {"n", "x̄", "ȳ", "Σx²", "Σx", "Σy²", "Σy", "Σxy",
            "Σx³", "Σx²y", "Σx⁴", "σx", "sx", "σy", "sy", "minX", "maxX",
            "minY", "maxY", "Q1", "Med", "Q3", "a", "b", "c", "r"};
}

std::optional<double> StatResult::get(const QString &k) const
{
    auto it = v.find(k);
    if (it == v.end()) return std::nullopt;
    return it->second;
}

static double median(const std::vector<double> &s, size_t lo, size_t hi)   // [lo, hi)
{
    size_t n = hi - lo;
    if (n == 0) return NAN;
    if (n % 2) return s[lo + n / 2];
    return 0.5 * (s[lo + n / 2 - 1] + s[lo + n / 2]);
}

StatResult computeStats(const StatData &d, RegType t)
{
    StatResult R;
    R.type = t;
    const size_t N = d.x.size();
    const bool paired = regIsPaired(t);
    if (N == 0) throw CalcError(ErrKind::Math);
    if (paired && d.y.size() != N) throw CalcError(ErrKind::Math);
    auto fr = [&](size_t i) { return d.f.empty() ? 1.0 : d.f[i]; };

    double n = 0, sx = 0, sx2 = 0, sy = 0, sy2 = 0, sxy = 0, sx3 = 0, sx2y = 0, sx4 = 0;
    double minX = INFINITY, maxX = -INFINITY, minY = INFINITY, maxY = -INFINITY;
    bool intFreq = true;
    for (size_t i = 0; i < N; ++i) {
        double f = fr(i);
        if (f < 0 || !std::isfinite(f)) throw CalcError(ErrKind::Math);
        if (f == 0) continue;
        if (f != std::floor(f)) intFreq = false;
        double x = d.x[i];
        n += f;
        sx += f * x;
        sx2 += f * x * x;
        sx3 += f * x * x * x;
        sx4 += f * x * x * x * x;
        minX = std::min(minX, x);
        maxX = std::max(maxX, x);
        if (paired) {
            double y = d.y[i];
            sy += f * y;
            sy2 += f * y * y;
            sxy += f * x * y;
            sx2y += f * x * x * y;
            minY = std::min(minY, y);
            maxY = std::max(maxY, y);
        }
    }
    if (n == 0) throw CalcError(ErrKind::Math);
    const QString xbar = QStringLiteral("x̄"), ybar = QStringLiteral("ȳ");
    R.v["n"] = n;
    R.v[xbar] = sx / n;
    R.v["Σx"] = sx;
    R.v["Σx²"] = sx2;
    double varx = std::max(0.0, sx2 / n - (sx / n) * (sx / n));
    R.v["σx"] = std::sqrt(varx);
    if (n > 1) R.v["sx"] = std::sqrt(std::max(0.0, (sx2 - sx * sx / n) / (n - 1)));
    R.v["minX"] = minX;
    R.v["maxX"] = maxX;
    R.oneVarOrder = {xbar, "Σx", "Σx²", "σx", "sx", "n"};

    if (!paired) {
        // quartiles (calculator method: halves exclude the median when n is odd)
        if (intFreq && n <= 1e6) {
            std::vector<double> s;
            for (size_t i = 0; i < N; ++i)
                for (int k = 0; k < int(fr(i)); ++k) s.push_back(d.x[i]);
            std::sort(s.begin(), s.end());
            size_t m = s.size();
            R.v["Med"] = median(s, 0, m);
            if (m == 1) {
                R.v["Q1"] = R.v["Q3"] = s[0];
            } else {
                R.v["Q1"] = median(s, 0, m / 2);
                R.v["Q3"] = median(s, (m + 1) / 2, m);
            }
            R.quartiles = true;
        }
        R.oneVarOrder << "minX" << "Q1" << "Med" << "Q3" << "maxX";
        return R;
    }

    R.v[ybar] = sy / n;
    R.v["Σy"] = sy;
    R.v["Σy²"] = sy2;
    R.v["Σxy"] = sxy;
    R.v["Σx³"] = sx3;
    R.v["Σx²y"] = sx2y;
    R.v["Σx⁴"] = sx4;
    R.v["σy"] = std::sqrt(std::max(0.0, sy2 / n - (sy / n) * (sy / n)));
    if (n > 1) R.v["sy"] = std::sqrt(std::max(0.0, (sy2 - sy * sy / n) / (n - 1)));
    R.v["minY"] = minY;
    R.v["maxY"] = maxY;
    R.oneVarOrder = {xbar, "Σx", "Σx²", "σx", "sx", "n"};
    R.pairedOrder = {ybar, "Σy", "Σy²", "σy", "sy", "Σxy", "Σx³", "Σx²y",
                     "Σx⁴", "minX", "maxX", "minY", "maxY"};

    if (t == RegType::Quadratic) {
        // least squares on 1, x, x²
        double Sxx = sx2 - sx * sx / n;
        double Sxy = sxy - sx * sy / n;
        double Sxx2 = sx3 - sx * sx2 / n;
        double Sx2x2 = sx4 - sx2 * sx2 / n;
        double Sx2y = sx2y - sx2 * sy / n;
        double den = Sxx * Sx2x2 - Sxx2 * Sxx2;
        if (den == 0) throw CalcError(ErrKind::Math);
        double b = (Sxy * Sx2x2 - Sx2y * Sxx2) / den;
        double c = (Sx2y * Sxx - Sxy * Sxx2) / den;
        double a = sy / n - b * sx / n - c * sx2 / n;
        R.v["a"] = a;
        R.v["b"] = b;
        R.v["c"] = c;
        R.regOrder = {"a", "b", "c"};
        return R;
    }

    // linearisable models: Y = A + B·X on transformed data
    auto tx = [&](double x) {
        switch (t) {
        case RegType::Log: case RegType::Power:
            if (x <= 0) throw CalcError(ErrKind::Math);
            return std::log(x);
        case RegType::Inverse:
            if (x == 0) throw CalcError(ErrKind::Math);
            return 1.0 / x;
        default: return x;
        }
    };
    auto ty = [&](double y) {
        switch (t) {
        case RegType::ExpE: case RegType::ExpAB: case RegType::Power:
            if (y <= 0) throw CalcError(ErrKind::Math);
            return std::log(y);
        default: return y;
        }
    };
    double tn = 0, tX = 0, tY = 0, tXX = 0, tYY = 0, tXY = 0;
    for (size_t i = 0; i < N; ++i) {
        double f = fr(i);
        if (f == 0) continue;
        double X = tx(d.x[i]), Y = ty(d.y[i]);
        tn += f; tX += f * X; tY += f * Y; tXX += f * X * X; tYY += f * Y * Y; tXY += f * X * Y;
    }
    double Sxx = tXX - tX * tX / tn, Syy = tYY - tY * tY / tn, Sxy = tXY - tX * tY / tn;
    if (Sxx == 0) throw CalcError(ErrKind::Math);
    double B = Sxy / Sxx;
    double A = tY / tn - B * tX / tn;
    double r = (Syy == 0) ? NAN : Sxy / std::sqrt(Sxx * Syy);
    double a = A, b = B;
    if (t == RegType::ExpE || t == RegType::Power) a = std::exp(A);
    if (t == RegType::ExpAB) { a = std::exp(A); b = std::exp(B); }
    R.v["a"] = a;
    R.v["b"] = b;
    if (std::isfinite(r)) R.v["r"] = r;
    R.regOrder = {"a", "b", "r"};
    return R;
}

double StatResult::estimateY(double x) const
{
    double a = v.at("a"), b = v.at("b");
    switch (type) {
    case RegType::Linear: return a + b * x;
    case RegType::Quadratic: return a + b * x + v.at("c") * x * x;
    case RegType::Log:
        if (x <= 0) throw CalcError(ErrKind::Math);
        return a + b * std::log(x);
    case RegType::ExpE: return a * std::exp(b * x);
    case RegType::ExpAB: return a * std::pow(b, x);
    case RegType::Power:
        if (x <= 0 && b != std::floor(b)) throw CalcError(ErrKind::Math);
        return a * std::pow(x, b);
    case RegType::Inverse:
        if (x == 0) throw CalcError(ErrKind::Math);
        return a + b / x;
    default: throw CalcError(ErrKind::Syntax);
    }
}

std::vector<double> StatResult::estimateX(double y) const
{
    double a = v.at("a"), b = v.at("b");
    auto need = [](bool ok) { if (!ok) throw CalcError(ErrKind::Math); };
    switch (type) {
    case RegType::Linear: need(b != 0); return {(y - a) / b};
    case RegType::Quadratic: {
        double c = v.at("c");
        need(c != 0);
        double D = b * b - 4 * c * (a - y);
        need(D >= 0);
        double s = std::sqrt(D);
        return {(-b + s) / (2 * c), (-b - s) / (2 * c)};
    }
    case RegType::Log: need(b != 0); return {std::exp((y - a) / b)};
    case RegType::ExpE: need(b != 0 && y / a > 0); return {std::log(y / a) / b};
    case RegType::ExpAB: need(b > 0 && b != 1 && y / a > 0); return {std::log(y / a) / std::log(b)};
    case RegType::Power: need(b != 0 && y / a > 0); return {std::pow(y / a, 1.0 / b)};
    case RegType::Inverse: need(y != a); return {b / (y - a)};
    default: throw CalcError(ErrKind::Syntax);
    }
}
