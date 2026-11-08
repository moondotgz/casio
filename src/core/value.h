#pragma once
#include <complex>
#include <vector>

using cplx = std::complex<double>;

// A calculator value: complex scalar, matrix (rows x cols) or vector (n x 1).
class Value {
public:
    enum class Kind { Scalar, Matrix, Vector };

    Kind kind = Kind::Scalar;
    cplx z{0.0, 0.0};
    int rows = 0, cols = 0;
    std::vector<cplx> m;   // row-major

    Value() = default;
    Value(double d) : z(d, 0.0) {}
    Value(cplx c) : z(c) {}

    static Value matrix(int r, int c);
    static Value vector(int n);

    bool isScalar() const { return kind == Kind::Scalar; }
    bool isMatrix() const { return kind == Kind::Matrix; }
    bool isVector() const { return kind == Kind::Vector; }
    bool isReal() const { return isScalar() && z.imag() == 0.0; }
    double re() const { return z.real(); }
    int size() const { return rows; }   // vector length

    cplx &at(int r, int c) { return m[size_t(r) * cols + c]; }
    const cplx &at(int r, int c) const { return m[size_t(r) * cols + c]; }
    cplx &operator[](int i) { return m[size_t(i)]; }
    const cplx &operator[](int i) const { return m[size_t(i)]; }
};

// Matrix / vector algebra. All throw CalcError on dimension or math problems.
Value valAdd(const Value &a, const Value &b);
Value valSub(const Value &a, const Value &b);
Value valMul(const Value &a, const Value &b);
Value valDiv(const Value &a, const Value &b);
Value valNeg(const Value &a);

Value matIdentity(int n);
cplx matDet(const Value &a);
Value matTranspose(const Value &a);
Value matInverse(const Value &a);
Value matRowEchelon(const Value &a, bool reduced);
Value matPower(const Value &a, long long n);

cplx vecDot(const Value &a, const Value &b);
Value vecCross(const Value &a, const Value &b);
double vecNorm(const Value &a);
