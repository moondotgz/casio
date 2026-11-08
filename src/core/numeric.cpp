#include "numeric.h"
#include "error.h"
#include <algorithm>
#include <cmath>
#include <numeric>
#include <queue>

namespace num {

bool isInteger(double x) { return std::isfinite(x) && x == std::floor(x); }

bool smallRational(double y, long long &p, long long &q, long long maxQ, double tol)
{
    if (!std::isfinite(y)) return false;
    double x = y;
    long long h0 = 0, h1 = 1, k0 = 1, k1 = 0;
    for (int it = 0; it < 40; ++it) {
        double a = std::floor(x);
        if (std::abs(a) > 1e15) return false;
        long long ai = static_cast<long long>(a);
        long long h2 = ai * h1 + h0, k2 = ai * k1 + k0;
        if (k2 > maxQ) return false;
        h0 = h1; h1 = h2; k0 = k1; k1 = k2;
        if (std::abs(double(h1) / double(k1) - y) <= tol * std::max(1.0, std::abs(y))) {
            p = h1;
            q = k1;
            return true;
        }
        double frac = x - a;
        if (frac == 0) break;
        x = 1.0 / frac;
    }
    return false;
}

// ---------------------------------------------------------------- integration (adaptive Gauss–Kronrod 7/15)

namespace {
const double xgk[8] = {0.991455371120812639206854697526329, 0.949107912342758524526189684047851,
                       0.864864423359769072789712788640926, 0.741531185599394439863864773280788,
                       0.586087235467691130294144845693013, 0.405845151377397166906606412076961,
                       0.207784955007898467600689403773245, 0.0};
const double wgk[8] = {0.022935322010529224963732008058970, 0.063092092629978553290700663189204,
                       0.104790010322250183839876322541518, 0.140653259715525918745189590510238,
                       0.169004726639267902826583426598550, 0.190350578064785409913256402421014,
                       0.204432940075298892414161999234649, 0.209482141084727828012999174891714};
const double wg[4] = {0.129484966168869693270611432679082, 0.279705391489276667901467771423780,
                      0.381830050505118944950369775488975, 0.417959183673469387755102040816327};

struct Seg {
    double a, b, val, err;
    bool operator<(const Seg &o) const { return err < o.err; }
};

Seg gk15(const RealFn &f, double a, double b)
{
    double c = 0.5 * (a + b), h = 0.5 * (b - a);
    double fc = f(c);
    double rk = fc * wgk[7], rg = fc * wg[3];
    for (int j = 0; j < 7; ++j) {
        double dx = h * xgk[j];
        double f1 = f(c - dx), f2 = f(c + dx);
        rk += wgk[j] * (f1 + f2);
        if (j % 2 == 1) rg += wg[j / 2] * (f1 + f2);
    }
    return {a, b, rk * h, std::abs((rk - rg) * h)};
}
} // namespace

double integrate(const RealFn &f, double a, double b)
{
    if (a == b) return 0;
    std::priority_queue<Seg> q;
    // start with a few panels to catch oscillating integrands
    const int initial = 8;
    double total = 0, err = 0;
    for (int i = 0; i < initial; ++i) {
        double s0 = a + (b - a) * i / initial, s1 = a + (b - a) * (i + 1) / initial;
        Seg s = gk15(f, s0, s1);
        total += s.val;
        err += s.err;
        q.push(s);
    }
    int evals = initial;
    while (err > std::max(1e-13, 1e-12 * std::abs(total)) && evals < 2000) {
        Seg s = q.top();
        q.pop();
        double m = 0.5 * (s.a + s.b);
        if (m <= s.a || m >= s.b) break;   // cannot split further
        Seg l = gk15(f, s.a, m), r = gk15(f, m, s.b);
        total += l.val + r.val - s.val;
        err += l.err + r.err - s.err;
        q.push(l);
        q.push(r);
        evals += 2;
    }
    if (!std::isfinite(total)) throw CalcError(ErrKind::Math);
    if (evals >= 2000 && err > 1e-6 * std::max(1.0, std::abs(total))) throw CalcError(ErrKind::TimeOut);
    return total;
}

// ---------------------------------------------------------------- differentiation (Ridders' extrapolation)

double differentiate(const RealFn &f, double x)
{
    const int N = 10;
    const double con = 1.4, con2 = con * con, safe = 2.0;
    double h = 1e-2 * std::max(1.0, std::abs(x));
    double a[N][N];
    a[0][0] = (f(x + h) - f(x - h)) / (2 * h);
    double err = 1e300, ans = a[0][0];
    for (int i = 1; i < N; ++i) {
        h /= con;
        a[0][i] = (f(x + h) - f(x - h)) / (2 * h);
        double fac = con2;
        for (int j = 1; j <= i; ++j) {
            a[j][i] = (a[j - 1][i] * fac - a[j - 1][i - 1]) / (fac - 1);
            fac *= con2;
            double errt = std::max(std::abs(a[j][i] - a[j - 1][i]), std::abs(a[j][i] - a[j - 1][i - 1]));
            if (errt <= err) {
                err = errt;
                ans = a[j][i];
            }
        }
        if (std::abs(a[i][i] - a[i - 1][i - 1]) >= safe * err) break;
    }
    if (!std::isfinite(ans)) throw CalcError(ErrKind::Math);
    // snap tiny noise around integers (e.g. 2.0000000000003 → 2)
    double r = std::round(ans);
    if (std::abs(ans - r) < 1e-10 * std::max(1.0, std::abs(ans))) ans = r;
    return ans;
}

// ---------------------------------------------------------------- equation solver

SolveResult solve(const RealFn &g, double x0)
{
    auto safe = [&](double x, double &y) {
        try {
            y = g(x);
            return std::isfinite(y);
        } catch (...) {
            return false;
        }
    };
    auto finish = [&](double x) -> SolveResult {
        double y = 0;
        safe(x, y);
        if (std::abs(x) < 1e-15) x = 0;
        return {x, y};
    };

    // 1) damped Newton from the initial guess
    {
        double x = x0, fx;
        if (safe(x, fx)) {
            for (int it = 0; it < 200; ++it) {
                if (fx == 0) return finish(x);
                double h = 1e-7 * std::max(1.0, std::abs(x));
                double f1, f2;
                if (!safe(x + h, f1) || !safe(x - h, f2)) break;
                double d = (f1 - f2) / (2 * h);
                if (d == 0 || !std::isfinite(d)) break;
                double step = fx / d, xn = x - step, fn;
                int k = 0;
                while ((!safe(xn, fn) || std::abs(fn) > std::abs(fx)) && k < 30) {
                    step *= 0.5;
                    xn = x - step;
                    ++k;
                }
                if (k == 30) break;
                x = xn;
                fx = fn;
                if (std::abs(step) <= 1e-15 * std::max(1.0, std::abs(x))) {
                    return finish(x);
                }
            }
            if (std::abs(fx) < 1e-12) return finish(x);
        }
    }

    // 2) scan outward from x0 for a sign change, then bisect
    double prevL = x0, prevR = x0, fL, fR, fx0;
    bool okL = safe(x0, fx0), okR = okL;
    fL = fR = fx0;
    for (int k = 0; k < 400; ++k) {
        double s = 1e-3 * std::pow(1.15, k);
        double xl = x0 - s, xr = x0 + s, yl, yr;
        bool gl = safe(xl, yl), gr = safe(xr, yr);
        auto bisect = [&](double a, double fa, double b, double) {
            for (int it = 0; it < 200; ++it) {
                double m = 0.5 * (a + b), fm;
                if (!safe(m, fm)) break;
                if (fm == 0 || (b - a) <= 1e-15 * std::max(1.0, std::abs(m))) return m;
                if ((fm < 0) == (fa < 0)) { a = m; fa = fm; } else { b = m; }
            }
            return 0.5 * (a + b);
        };
        if (gr && okR && ((yr < 0) != (fR < 0) || yr == 0)) {
            double r = bisect(prevR, fR, xr, yr);
            double fr;
            if (safe(r, fr) && std::abs(fr) < 1e-6 * std::max(1.0, std::abs(fR) + std::abs(yr))) return finish(r);
        }
        if (gl && okL && ((yl < 0) != (fL < 0) || yl == 0)) {
            double r = bisect(xl, yl, prevL, fL);
            double fr;
            if (safe(r, fr) && std::abs(fr) < 1e-6 * std::max(1.0, std::abs(fL) + std::abs(yl))) return finish(r);
        }
        prevL = xl; prevR = xr;
        fL = yl; fR = yr;
        okL = gl; okR = gr;
        if (s > 1e12) break;
    }
    throw CalcError(ErrKind::CantSolve);
}

// ---------------------------------------------------------------- polynomials

namespace {

using Poly = std::vector<double>;   // highest first

cplx hornerC(const Poly &p, cplx z)
{
    cplx r = 0;
    for (double c : p) r = r * z + c;
    return r;
}

std::vector<long long> divisors(long long n)
{
    n = std::llabs(n);
    std::vector<long long> d;
    for (long long i = 1; i * i <= n; ++i)
        if (n % i == 0) {
            d.push_back(i);
            if (i != n / i) d.push_back(n / i);
        }
    return d;
}

// Find a rational root p/q of an integer polynomial (exact arithmetic).
bool rationalRoot(const Poly &P, long long &rp, long long &rq)
{
    for (double c : P)
        if (!isInteger(c) || std::abs(c) > 1e6) return false;
    long long a0 = static_cast<long long>(P.back()), an = static_cast<long long>(P.front());
    if (a0 == 0) { rp = 0; rq = 1; return true; }
    auto ps = divisors(a0), qs = divisors(an);
    int n = int(P.size()) - 1;
    for (long long q : qs)
        for (long long p0 : ps)
            for (int sgn : {1, -1}) {
                long long p = sgn * p0;
                if (std::gcd(p0, q) != 1) continue;
                __int128 s = 0;
                // q^n P(p/q) = Σ c_i p^(n-i) q^i  (c_0 is the leading coefficient)
                for (int i = 0; i <= n; ++i) {
                    __int128 term = static_cast<long long>(P[size_t(i)]);
                    for (int k = 0; k < n - i; ++k) term *= p;
                    for (int k = 0; k < i; ++k) term *= q;
                    s += term;
                }
                if (s == 0) { rp = p; rq = q; return true; }
            }
    return false;
}

Poly deflateRational(const Poly &P, long long p, long long q)
{
    // divide by (q x - p): exact for integer polynomials with that root
    Poly out;
    double carry = 0;
    for (size_t i = 0; i + 1 < P.size(); ++i) {
        double c = (P[i] + carry) / double(q);
        out.push_back(c);
        carry = c * double(p);
    }
    return out;
}

Poly deflateReal(const Poly &P, double r)
{
    Poly out;
    double acc = 0;
    for (size_t i = 0; i + 1 < P.size(); ++i) {
        acc = acc * r + P[i];
        out.push_back(acc);
    }
    return out;
}

void quadratic(double a, double b, double c, std::vector<cplx> &out)
{
    double D = b * b - 4 * a * c;
    // treat round-off around a double root as zero
    if (std::abs(D) <= 1e-14 * (b * b + std::abs(4 * a * c))) D = 0;
    if (D >= 0) {
        double sd = std::sqrt(D);
        double r1 = (-b + sd) / (2 * a), r2 = (-b - sd) / (2 * a);
        out.push_back(r1);
        out.push_back(r2);
    } else {
        double re = -b / (2 * a), im = std::sqrt(-D) / (2 * std::abs(a));
        out.push_back(cplx(re, im));
        out.push_back(cplx(re, -im));
    }
}

std::vector<cplx> durandKerner(const Poly &P)
{
    int n = int(P.size()) - 1;
    Poly m = P;
    for (double &c : m) c /= P[0];
    double R = 1;
    for (size_t i = 1; i < m.size(); ++i) R = std::max(R, 1 + std::abs(m[i]));
    std::vector<cplx> z(static_cast<size_t>(n));
    for (int k = 0; k < n; ++k) z[size_t(k)] = std::polar(R * 0.9, 2 * M_PI * k / n + 0.4);
    for (int it = 0; it < 2000; ++it) {
        double delta = 0;
        for (int i = 0; i < n; ++i) {
            cplx den = 1;
            for (int j = 0; j < n; ++j)
                if (j != i) den *= (z[size_t(i)] - z[size_t(j)]);
            if (std::abs(den) == 0) den = 1e-12;
            cplx d = hornerC(m, z[size_t(i)]) / den;
            z[size_t(i)] -= d;
            delta = std::max(delta, std::abs(d));
        }
        if (delta < 1e-16) break;
    }
    // Newton polish
    for (auto &r : z)
        for (int it = 0; it < 5; ++it) {
            cplx f = 0, df = 0;
            for (double c : m) { df = df * r + f; f = f * r + c; }
            if (std::abs(df) == 0) break;
            r -= f / df;
        }
    return z;
}

double polishReal(const Poly &P, double x)
{
    for (int it = 0; it < 50; ++it) {
        double f = 0, df = 0;
        for (double c : P) { df = df * x + f; f = f * x + c; }
        if (df == 0) break;
        double s = f / df;
        x -= s;
        if (std::abs(s) <= 1e-16 * std::max(1.0, std::abs(x))) break;
    }
    return x;
}

void solveRest(Poly P, std::vector<cplx> &out)
{
    while (P.size() > 3) {
        auto z = durandKerner(P);
        // pick the most real root
        size_t best = 0;
        for (size_t i = 1; i < z.size(); ++i)
            if (std::abs(z[i].imag()) < std::abs(z[best].imag())) best = i;
        if (std::abs(z[best].imag()) < 1e-7 * std::max(1.0, std::abs(z[best]))) {
            double r = polishReal(P, z[best].real());
            out.push_back(r);
            P = deflateReal(P, r);
        } else {
            for (auto &r : z) out.push_back(r);
            return;
        }
    }
    if (P.size() == 3) quadratic(P[0], P[1], P[2], out);
    else if (P.size() == 2) out.push_back(-P[1] / P[0]);
}

} // namespace

std::vector<cplx> polyRoots(const std::vector<double> &coeffs)
{
    Poly P = coeffs;
    while (!P.empty() && P.front() == 0) P.erase(P.begin());
    if (P.size() < 2) throw CalcError(ErrKind::Math);
    std::vector<cplx> roots;
    long long p, q;
    while (P.size() > 3 && rationalRoot(P, p, q)) {
        roots.push_back(double(p) / double(q));
        P = deflateRational(P, p, q);
    }
    solveRest(P, roots);
    for (auto &r : roots) {
        double re = r.real(), im = r.imag();
        double sc = std::max(1.0, std::abs(r));
        if (std::abs(im) < 1e-12 * sc) im = 0;
        if (std::abs(re) < 1e-14 * sc) re = 0;
        r = cplx(re, im);
    }
    std::stable_sort(roots.begin(), roots.end(), [](const cplx &a, const cplx &b) {
        bool ra = a.imag() == 0, rb = b.imag() == 0;
        if (ra != rb) return ra;
        if (ra) return a.real() > b.real();
        if (a.real() != b.real()) return a.real() > b.real();
        return a.imag() > b.imag();
    });
    return roots;
}

LinStatus solveLinear(std::vector<std::vector<double>> A, std::vector<double> b, std::vector<double> &x)
{
    const int n = int(A.size());
    double scale = 0;
    for (auto &row : A) for (double v : row) scale = std::max(scale, std::abs(v));
    double eps = 1e-12 * std::max(1.0, scale);
    std::vector<int> pivCol;
    int row = 0;
    for (int c = 0; c < n && row < n; ++c) {
        int piv = row;
        for (int r = row + 1; r < n; ++r)
            if (std::abs(A[size_t(r)][size_t(c)]) > std::abs(A[size_t(piv)][size_t(c)])) piv = r;
        if (std::abs(A[size_t(piv)][size_t(c)]) <= eps) continue;
        std::swap(A[size_t(piv)], A[size_t(row)]);
        std::swap(b[size_t(piv)], b[size_t(row)]);
        for (int r = 0; r < n; ++r) {
            if (r == row) continue;
            double f = A[size_t(r)][size_t(c)] / A[size_t(row)][size_t(c)];
            if (f == 0) continue;
            for (int k = 0; k < n; ++k) A[size_t(r)][size_t(k)] -= f * A[size_t(row)][size_t(k)];
            b[size_t(r)] -= f * b[size_t(row)];
        }
        pivCol.push_back(c);
        ++row;
    }
    if (row < n) {
        double bs = 0;
        for (double v : b) bs = std::max(bs, std::abs(v));
        for (int r = row; r < n; ++r)
            if (std::abs(b[size_t(r)]) > 1e-10 * std::max(1.0, bs)) return LinStatus::NoSolution;
        return LinStatus::Infinite;
    }
    x.assign(size_t(n), 0);
    for (int r = 0; r < n; ++r) {
        double v = b[size_t(r)] / A[size_t(r)][size_t(pivCol[size_t(r)])];
        if (std::abs(v) < 1e-15 * std::max(1.0, scale)) v = 0;
        x[size_t(pivCol[size_t(r)])] = v;
    }
    return LinStatus::Unique;
}

// ---------------------------------------------------------------- distributions

double normalPdf(double x, double s, double m)
{
    if (s <= 0) throw CalcError(ErrKind::Math);
    double z = (x - m) / s;
    return std::exp(-0.5 * z * z) / (s * std::sqrt(2 * M_PI));
}

double normalCdf(double lo, double hi, double s, double m)
{
    if (s <= 0 || lo > hi) throw CalcError(ErrKind::Math);
    auto Phi = [&](double x) { return 0.5 * std::erfc(-(x - m) / (s * M_SQRT2)); };
    return Phi(hi) - Phi(lo);
}

double invNormalStd(double p)
{
    if (p <= 0 || p >= 1) throw CalcError(ErrKind::Math);
    static const double a[] = {-3.969683028665376e+01, 2.209460984245205e+02, -2.759285104469687e+02,
                               1.383577518672690e+02, -3.066479806614716e+01, 2.506628277459239e+00};
    static const double b[] = {-5.447609879822406e+01, 1.615858368580409e+02, -1.556989798598866e+02,
                               6.680131188771972e+01, -1.328068155288572e+01};
    static const double c[] = {-7.784894002430293e-03, -3.223964580411365e-01, -2.400758277161838e+00,
                               -2.549732539343734e+00, 4.374664141464968e+00, 2.938163982698783e+00};
    static const double d[] = {7.784695709041462e-03, 3.224671290700398e-01, 2.445134137142996e+00,
                               3.754408661907416e+00};
    const double plow = 0.02425;
    double x;
    if (p < plow) {
        double q = std::sqrt(-2 * std::log(p));
        x = (((((c[0] * q + c[1]) * q + c[2]) * q + c[3]) * q + c[4]) * q + c[5]) /
            ((((d[0] * q + d[1]) * q + d[2]) * q + d[3]) * q + 1);
    } else if (p <= 1 - plow) {
        double q = p - 0.5, r = q * q;
        x = (((((a[0] * r + a[1]) * r + a[2]) * r + a[3]) * r + a[4]) * r + a[5]) * q /
            (((((b[0] * r + b[1]) * r + b[2]) * r + b[3]) * r + b[4]) * r + 1);
    } else {
        double q = std::sqrt(-2 * std::log(1 - p));
        x = -(((((c[0] * q + c[1]) * q + c[2]) * q + c[3]) * q + c[4]) * q + c[5]) /
            ((((d[0] * q + d[1]) * q + d[2]) * q + d[3]) * q + 1);
    }
    for (int i = 0; i < 2; ++i) {   // Halley refinement
        double e = 0.5 * std::erfc(-x / M_SQRT2) - p;
        double u = e * std::sqrt(2 * M_PI) * std::exp(x * x / 2);
        x = x - u / (1 + x * u / 2);
    }
    return x;
}

static void checkBinom(double n, double p)
{
    if (!isInteger(n) || n < 0 || n > 1e5 || p < 0 || p > 1) throw CalcError(ErrKind::Math);
}

double binomialPdf(double x, double n, double p)
{
    checkBinom(n, p);
    if (!isInteger(x)) throw CalcError(ErrKind::Math);
    if (x < 0 || x > n) return 0;
    if (p == 0) return x == 0 ? 1 : 0;
    if (p == 1) return x == n ? 1 : 0;
    if (n <= 1000) return combinations(n, x) * std::pow(p, x) * std::pow(1 - p, n - x);
    return std::exp(std::lgamma(n + 1) - std::lgamma(x + 1) - std::lgamma(n - x + 1) + x * std::log(p) +
                    (n - x) * std::log1p(-p));
}

double binomialCdf(double x, double n, double p)
{
    checkBinom(n, p);
    if (!isInteger(x)) throw CalcError(ErrKind::Math);
    if (x < 0) return 0;
    double s = 0;
    for (double k = 0; k <= std::min(x, n); ++k) s += binomialPdf(k, n, p);
    return std::min(s, 1.0);
}

double poissonPdf(double x, double l)
{
    if (l <= 0 || l > 1e8) throw CalcError(ErrKind::Math);
    if (!isInteger(x)) throw CalcError(ErrKind::Math);
    if (x < 0) return 0;
    return std::exp(-l + x * std::log(l) - std::lgamma(x + 1));
}

double poissonCdf(double x, double l)
{
    if (l <= 0 || l > 1e8 || !isInteger(x)) throw CalcError(ErrKind::Math);
    if (x > 1e6) throw CalcError(ErrKind::Math);
    double s = 0;
    for (double k = 0; k <= x; ++k) s += poissonPdf(k, l);
    return std::min(s, 1.0);
}

// ---------------------------------------------------------------- combinatorics & number theory

double factorial(double n)
{
    if (!isInteger(n) || n < 0 || n > 69) throw CalcError(ErrKind::Math);
    double r = 1;
    for (int i = 2; i <= int(n); ++i) r *= i;
    return r;
}

double permutations(double n, double r)
{
    if (!isInteger(n) || !isInteger(r) || r < 0 || n < 0 || r > n || n >= 1e10) throw CalcError(ErrKind::Math);
    double p = 1;
    for (double k = 0; k < r; ++k) {
        p *= (n - k);
        if (p >= 1e100) throw CalcError(ErrKind::Math);
    }
    return p;
}

double combinations(double n, double r)
{
    if (!isInteger(n) || !isInteger(r) || r < 0 || n < 0 || r > n || n >= 1e10) throw CalcError(ErrKind::Math);
    r = std::min(r, n - r);
    double c = 1;
    for (double k = 0; k < r; ++k) {
        c = c * (n - k) / (k + 1);
        if (c >= 1e100) throw CalcError(ErrKind::Math);
    }
    if (c < 9e15) c = std::round(c);
    return c;
}

double gcd(double a, double b)
{
    if (!isInteger(a) || !isInteger(b) || std::abs(a) >= 1e10 || std::abs(b) >= 1e10) throw CalcError(ErrKind::Math);
    long long x = std::llabs(static_cast<long long>(a)), y = std::llabs(static_cast<long long>(b));
    return double(std::gcd(x, y));
}

double lcm(double a, double b)
{
    if (!isInteger(a) || !isInteger(b) || std::abs(a) >= 1e10 || std::abs(b) >= 1e10) throw CalcError(ErrKind::Math);
    long long x = std::llabs(static_cast<long long>(a)), y = std::llabs(static_cast<long long>(b));
    if (x == 0 || y == 0) return 0;
    return double(std::lcm(x, y));
}

std::vector<std::pair<qint64, int>> primeFactors(qint64 n, qint64 &rest)
{
    std::vector<std::pair<qint64, int>> f;
    rest = 1;
    if (n < 2) return f;
    for (qint64 p = 2; p * p <= n; p += (p == 2 ? 1 : 2)) {
        int e = 0;
        while (n % p == 0) { n /= p; ++e; }
        if (e) f.push_back({p, e});
    }
    if (n > 1) f.push_back({n, 1});
    return f;
}

} // namespace num
