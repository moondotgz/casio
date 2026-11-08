#include "value.h"
#include "error.h"
#include <cmath>

Value Value::matrix(int r, int c)
{
    Value v;
    v.kind = Kind::Matrix;
    v.rows = r;
    v.cols = c;
    v.m.assign(size_t(r) * c, cplx(0, 0));
    return v;
}

Value Value::vector(int n)
{
    Value v;
    v.kind = Kind::Vector;
    v.rows = n;
    v.cols = 1;
    v.m.assign(size_t(n), cplx(0, 0));
    return v;
}

static void requireSameShape(const Value &a, const Value &b)
{
    if (a.kind != b.kind || a.rows != b.rows || a.cols != b.cols)
        throw CalcError(ErrKind::Dimension);
}

static Value elementwise(const Value &a, const Value &b, cplx (*op)(cplx, cplx))
{
    if (a.isScalar() && b.isScalar())
        return Value(op(a.z, b.z));
    if (a.isScalar() || b.isScalar())
        throw CalcError(ErrKind::Dimension);   // scalar +/- matrix is not allowed
    requireSameShape(a, b);
    Value r = a;
    for (size_t i = 0; i < r.m.size(); ++i)
        r.m[i] = op(a.m[i], b.m[i]);
    return r;
}

Value valAdd(const Value &a, const Value &b)
{
    return elementwise(a, b, [](cplx x, cplx y) { return x + y; });
}

Value valSub(const Value &a, const Value &b)
{
    return elementwise(a, b, [](cplx x, cplx y) { return x - y; });
}

Value valNeg(const Value &a)
{
    Value r = a;
    if (r.isScalar())
        r.z = -r.z;
    for (auto &e : r.m)
        e = -e;
    return r;
}

static Value scale(const Value &a, cplx s)
{
    Value r = a;
    for (auto &e : r.m)
        e *= s;
    return r;
}

Value valMul(const Value &a, const Value &b)
{
    if (a.isScalar() && b.isScalar())
        return Value(a.z * b.z);
    if (a.isScalar())
        return scale(b, a.z);
    if (b.isScalar())
        return scale(a, b.z);
    if (a.isVector() && b.isVector())
        throw CalcError(ErrKind::Dimension);   // use DotP / CrossP
    // matrix x matrix, matrix x vector (column), vector x matrix (row vector, 1 x n)
    int ar = a.rows, ac = a.cols, br = b.rows, bc = b.cols;
    if (a.isVector()) { ar = 1; ac = a.rows; }
    if (ac != br)
        throw CalcError(ErrKind::Dimension);
    Value r = (b.isVector()) ? Value::vector(ar) : Value::matrix(ar, bc);
    if (a.isVector() && !b.isVector()) {
        r = (bc == 1) ? Value(0.0) : Value::vector(bc);
        if (bc == 1) {
            cplx s = 0;
            for (int k = 0; k < ac; ++k) s += a.m[size_t(k)] * b.at(k, 0);
            return Value(s);
        }
        for (int j = 0; j < bc; ++j) {
            cplx s = 0;
            for (int k = 0; k < ac; ++k) s += a.m[size_t(k)] * b.at(k, j);
            r.m[size_t(j)] = s;
        }
        return r;
    }
    for (int i = 0; i < ar; ++i)
        for (int j = 0; j < bc; ++j) {
            cplx s = 0;
            for (int k = 0; k < ac; ++k)
                s += a.at(i, k) * b.m[size_t(k) * bc + j];
            r.m[size_t(i) * bc + j] = s;
        }
    return r;
}

Value valDiv(const Value &a, const Value &b)
{
    if (!b.isScalar())
        throw CalcError(ErrKind::Dimension);
    if (b.z == cplx(0, 0))
        throw CalcError(ErrKind::Math);
    if (a.isScalar())
        return Value(a.z / b.z);
    return scale(a, cplx(1, 0) / b.z);
}

Value matIdentity(int n)
{
    if (n < 1 || n > 4)
        throw CalcError(ErrKind::Argument);
    Value r = Value::matrix(n, n);
    for (int i = 0; i < n; ++i) r.at(i, i) = 1;
    return r;
}

cplx matDet(const Value &a)
{
    if (!a.isMatrix() || a.rows != a.cols)
        throw CalcError(ErrKind::Dimension);
    int n = a.rows;
    Value t = a;
    cplx det = 1;
    for (int c = 0; c < n; ++c) {
        int piv = c;
        for (int r = c + 1; r < n; ++r)
            if (std::abs(t.at(r, c)) > std::abs(t.at(piv, c))) piv = r;
        if (std::abs(t.at(piv, c)) == 0.0) return 0;
        if (piv != c) {
            for (int k = 0; k < n; ++k) std::swap(t.at(piv, k), t.at(c, k));
            det = -det;
        }
        det *= t.at(c, c);
        for (int r = c + 1; r < n; ++r) {
            cplx f = t.at(r, c) / t.at(c, c);
            for (int k = c; k < n; ++k) t.at(r, k) -= f * t.at(c, k);
        }
    }
    return det;
}

Value matTranspose(const Value &a)
{
    if (a.isVector()) {   // column vector -> 1 x n matrix
        Value r = Value::matrix(1, a.rows);
        for (int i = 0; i < a.rows; ++i) r.at(0, i) = a.m[size_t(i)];
        return r;
    }
    if (!a.isMatrix())
        throw CalcError(ErrKind::Dimension);
    Value r = Value::matrix(a.cols, a.rows);
    for (int i = 0; i < a.rows; ++i)
        for (int j = 0; j < a.cols; ++j) r.at(j, i) = a.at(i, j);
    return r;
}

Value matInverse(const Value &a)
{
    if (!a.isMatrix() || a.rows != a.cols)
        throw CalcError(ErrKind::Dimension);
    int n = a.rows;
    Value t = a, inv = matIdentity(n);
    double scaleMax = 0;
    for (auto &e : a.m) scaleMax = std::max(scaleMax, std::abs(e));
    for (int c = 0; c < n; ++c) {
        int piv = c;
        for (int r = c + 1; r < n; ++r)
            if (std::abs(t.at(r, c)) > std::abs(t.at(piv, c))) piv = r;
        if (std::abs(t.at(piv, c)) <= 1e-14 * std::max(1.0, scaleMax))
            throw CalcError(ErrKind::Math);   // singular
        for (int k = 0; k < n; ++k) {
            std::swap(t.at(piv, k), t.at(c, k));
            std::swap(inv.at(piv, k), inv.at(c, k));
        }
        cplx p = t.at(c, c);
        for (int k = 0; k < n; ++k) { t.at(c, k) /= p; inv.at(c, k) /= p; }
        for (int r = 0; r < n; ++r) {
            if (r == c) continue;
            cplx f = t.at(r, c);
            if (f == cplx(0, 0)) continue;
            for (int k = 0; k < n; ++k) {
                t.at(r, k) -= f * t.at(c, k);
                inv.at(r, k) -= f * inv.at(c, k);
            }
        }
    }
    return inv;
}

Value matRowEchelon(const Value &a, bool reduced)
{
    if (!a.isMatrix())
        throw CalcError(ErrKind::Dimension);
    Value t = a;
    int R = t.rows, C = t.cols, row = 0;
    double scaleMax = 0;
    for (auto &e : a.m) scaleMax = std::max(scaleMax, std::abs(e));
    double eps = 1e-12 * std::max(1.0, scaleMax);
    for (int c = 0; c < C && row < R; ++c) {
        int piv = row;
        for (int r = row + 1; r < R; ++r)
            if (std::abs(t.at(r, c)) > std::abs(t.at(piv, c))) piv = r;
        if (std::abs(t.at(piv, c)) <= eps) {
            for (int r = row; r < R; ++r) t.at(r, c) = 0;
            continue;
        }
        for (int k = 0; k < C; ++k) std::swap(t.at(piv, k), t.at(row, k));
        cplx p = t.at(row, c);
        for (int k = 0; k < C; ++k) t.at(row, k) /= p;
        for (int r = reduced ? 0 : row + 1; r < R; ++r) {
            if (r == row) continue;
            cplx f = t.at(r, c);
            for (int k = 0; k < C; ++k) t.at(r, k) -= f * t.at(row, k);
            t.at(r, c) = 0;
        }
        ++row;
    }
    for (auto &e : t.m) {
        if (std::abs(e.real()) <= eps) e.real(0);
        if (std::abs(e.imag()) <= eps) e.imag(0);
    }
    return t;
}

Value matPower(const Value &a, long long n)
{
    if (!a.isMatrix() || a.rows != a.cols)
        throw CalcError(ErrKind::Dimension);
    Value base = a;
    if (n < 0) { base = matInverse(a); n = -n; }
    Value result = matIdentity(a.rows);
    while (n > 0) {
        if (n & 1) result = valMul(result, base);
        n >>= 1;
        if (n) base = valMul(base, base);
    }
    return result;
}

cplx vecDot(const Value &a, const Value &b)
{
    if (!a.isVector() || !b.isVector() || a.rows != b.rows)
        throw CalcError(ErrKind::Dimension);
    cplx s = 0;
    for (int i = 0; i < a.rows; ++i) s += a.m[size_t(i)] * b.m[size_t(i)];
    return s;
}

Value vecCross(const Value &a, const Value &b)
{
    if (!a.isVector() || !b.isVector() || a.rows != b.rows || a.rows < 2)
        throw CalcError(ErrKind::Dimension);
    // 2-D vectors are treated as 3-D with z = 0 (as on the real calculator)
    auto g = [](const Value &v, int i) { return i < v.rows ? v.m[size_t(i)] : cplx(0, 0); };
    Value r = Value::vector(3);
    r.m[0] = g(a, 1) * g(b, 2) - g(a, 2) * g(b, 1);
    r.m[1] = g(a, 2) * g(b, 0) - g(a, 0) * g(b, 2);
    r.m[2] = g(a, 0) * g(b, 1) - g(a, 1) * g(b, 0);
    return r;
}

double vecNorm(const Value &a)
{
    double s = 0;
    for (auto &e : a.m) s += std::norm(e);
    return std::sqrt(s);
}
