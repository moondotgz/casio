#include "evaluator.h"
#include "constants.h"
#include "numeric.h"
#include <QSet>
#include <cmath>
#include <limits>

namespace {

constexpr double PI = 3.14159265358979323846;

struct VarGuard {   // temporarily binds a variable, restores on scope exit
    Context &c;
    QString name;
    Value saved;
    VarGuard(Context &ctx, const QString &n) : c(ctx), name(n), saved(ctx.vars.value(n)) {}
    ~VarGuard() { c.vars[name] = saved; }
};

const QSet<QString> &scalarVarNames()
{
    static const QSet<QString> s = {"A", "B", "C", "D", "E", "F", "M", "x", "y", "Ans", "PreAns"};
    return s;
}

} // namespace

// ---------------------------------------------------------------- Context

Context::Context()
{
    for (const auto &v : scalarVarNames()) vars[v] = Value(0.0);
}

Value Context::getVar(const QString &name, int pos) const
{
    if (varHook) {
        if (auto v = varHook(name)) return *v;
    }
    if (name == QStringLiteral("π")) return Value(PI);
    if (name == "e") return Value(M_E);
    if (name == "i") {
        if (!complexMode) throw CalcError(ErrKind::Syntax, pos);
        return Value(cplx(0, 1));
    }
    auto it = vars.find(name);
    if (it == vars.end()) {
        if (name.startsWith("Mat") || name.startsWith("Vct")) throw CalcError(ErrKind::Dimension, pos);
        throw CalcError(ErrKind::Syntax, pos);
    }
    return *it;
}

void Context::setVar(const QString &name, const Value &v) { vars[name] = v; }

void Context::pushAns(const Value &v)
{
    if (v.isMatrix()) { vars["MatAns"] = v; return; }
    if (v.isVector()) { vars["VctAns"] = v; return; }
    vars["PreAns"] = vars.value("Ans");
    vars["Ans"] = v;
}

// ---------------------------------------------------------------- helpers

double roundToDisplay(double x, const Settings &s)
{
    if (!std::isfinite(x) || x == 0) return x;
    if (s.format == NumFormat::Fix) {
        double f = std::pow(10.0, s.digits);
        double r = std::round(x * f) / f;
        return std::isfinite(r) ? r : x;
    }
    int sig = 10;
    if (s.format == NumFormat::Sci && s.digits > 0) sig = s.digits;
    return QString::number(x, 'e', sig - 1).toDouble();
}

double Evaluator::toRad(double a) const
{
    switch (ctx.settings.angle) {
    case AngleUnit::Deg: return a * PI / 180.0;
    case AngleUnit::Gra: return a * PI / 200.0;
    case AngleUnit::Rad: return a;
    }
    return a;
}

double Evaluator::fromRad(double r) const
{
    switch (ctx.settings.angle) {
    case AngleUnit::Deg: return r * 180.0 / PI;
    case AngleUnit::Gra: return r * 200.0 / PI;
    case AngleUnit::Rad: return r;
    }
    return r;
}

// Sine in the current angle unit, exact at multiples of a quarter turn (and the 30° family).
double Evaluator::sinU(double a) const
{
    double quarter = ctx.settings.angle == AngleUnit::Deg ? 90.0 : ctx.settings.angle == AngleUnit::Gra ? 100.0 : PI / 2;
    double full = 4 * quarter;
    if (std::abs(a) >= (ctx.settings.angle == AngleUnit::Rad ? 1.57e8 : ctx.settings.angle == AngleUnit::Deg ? 9e9 : 1e10))
        throw CalcError(ErrKind::Math);
    if (ctx.settings.angle == AngleUnit::Rad) {
        double q = a / quarter;
        double rq = std::round(q);
        if (std::abs(q - rq) < 1e-15 * std::max(1.0, std::abs(q)) * 4) {
            long long k = (static_cast<long long>(rq) % 4 + 4) % 4;
            static const double tbl[4] = {0, 1, 0, -1};
            return tbl[k];
        }
        return std::sin(a);
    }
    double r = std::fmod(a, full);
    if (r < 0) r += full;
    double q = r / quarter;
    if (q == std::floor(q)) {
        static const double tbl[4] = {0, 1, 0, -1};
        return tbl[int(q) % 4];
    }
    double sixth = quarter / 3.0;   // 30° family: ±1/2 exactly
    double s6 = r / sixth;
    if (s6 == std::floor(s6)) {
        int k = int(s6) % 12;
        if (k == 1 || k == 5) return 0.5;
        if (k == 7 || k == 11) return -0.5;
    }
    return std::sin(toRad(r));
}

double Evaluator::cosU(double a) const
{
    double quarter = ctx.settings.angle == AngleUnit::Deg ? 90.0 : ctx.settings.angle == AngleUnit::Gra ? 100.0 : PI / 2;
    return sinU(a + quarter);
}

cplx Evaluator::scalar(const Value &v, int pos) const
{
    if (!v.isScalar()) throw CalcError(ErrKind::Dimension, pos);
    return v.z;
}

double Evaluator::real(const Value &v, int pos) const
{
    cplx z = scalar(v, pos);
    if (z.imag() != 0) throw CalcError(ErrKind::Math, pos);
    return z.real();
}

static void cleanComponent(double &d, int pos)
{
    if (!std::isfinite(d)) throw CalcError(ErrKind::Math, pos);
    if (std::abs(d) >= 1e100) throw CalcError(ErrKind::Math, pos);
    if (std::abs(d) < 1e-99) d = 0;   // underflow → 0 (calculator range is 1e-99 .. 9.99e99)
}

Value Evaluator::check(const Value &v, int pos) const
{
    Value r = v;
    auto fix = [&](cplx &z) {
        double re = z.real(), im = z.imag();
        cleanComponent(re, pos);
        cleanComponent(im, pos);
        // drop round-off imaginary parts relative to the real part
        if (im != 0 && std::abs(im) < 1e-15 * std::abs(re)) im = 0;
        if (re != 0 && std::abs(re) < 1e-15 * std::abs(im)) re = 0;
        if (!ctx.complexMode && im != 0) throw CalcError(ErrKind::Math, pos);
        z = cplx(re, im);
    };
    if (r.isScalar()) fix(r.z);
    for (auto &e : r.m) fix(e);
    return r;
}

cplx Evaluator::powScalar(cplx a, cplx b, int pos) const
{
    if (a.imag() == 0 && b.imag() == 0) {
        double x = a.real(), y = b.real();
        if (x == 0) {
            if (y > 0) return 0;
            throw CalcError(ErrKind::Math, pos);
        }
        if (x < 0 && !num::isInteger(y)) {
            long long p, q;
            if (num::smallRational(y, p, q) && (q % 2 == 1)) {
                double mag = std::pow(-x, y);
                return (p % 2 == 0) ? mag : -mag;
            }
            if (!ctx.complexMode) throw CalcError(ErrKind::Math, pos);
            return std::pow(a, b);
        }
        return std::pow(x, y);
    }
    if (a == cplx(0, 0)) {
        if (b.real() > 0) return 0;
        throw CalcError(ErrKind::Math, pos);
    }
    if (b.imag() == 0 && num::isInteger(b.real()) && std::abs(b.real()) <= 4096) {
        long long n = static_cast<long long>(b.real());
        bool neg = n < 0;
        if (neg) n = -n;
        cplx r = 1, base = a;
        while (n) {
            if (n & 1) r *= base;
            base *= base;
            n >>= 1;
        }
        return neg ? cplx(1, 0) / r : r;
    }
    return std::pow(a, b);
}

Value Evaluator::power(const Value &a, const Value &b, int pos) const
{
    if (a.isScalar() && b.isScalar()) return Value(powScalar(a.z, b.z, pos));
    if (a.isMatrix() && b.isReal() && num::isInteger(b.re()) && std::abs(b.re()) <= 1e6)
        return matPower(a, static_cast<long long>(b.re()));
    throw CalcError(ErrKind::Dimension, pos);
}

// ---------------------------------------------------------------- evaluation

Value Evaluator::evaluate(const NodePtr &n)
{
    ctx.outputs.clear();
    ctx.forcedForm = ForcedForm::None;
    depth = 0;
    if (ctx.base) {
        if (n->type == Node::Type::Assign) {
            Value v(double(evalInt(n->kids[0])));
            ctx.setVar(n->op, v);
            return v;
        }
        return Value(double(evalInt(n)));
    }
    return check(eval(n), n->pos);
}

double Evaluator::evalRealAt(const NodePtr &n, const QString &var, double v)
{
    VarGuard g(ctx, var);
    ctx.vars[var] = Value(v);
    Value r = check(eval(n), n->pos);
    if (!r.isReal()) throw CalcError(ErrKind::Math, n->pos);
    return r.re();
}

Value Evaluator::eval(const NodePtr &n)
{
    struct DepthGuard {
        int &d;
        explicit DepthGuard(int &x) : d(x) { if (++d > 150) throw CalcError(ErrKind::Stack); }
        ~DepthGuard() { --d; }
    } dg(depth);

    switch (n->type) {
    case Node::Type::Num:
        if (!std::isfinite(n->num) || std::abs(n->num) >= 1e100) throw CalcError(ErrKind::Math, n->pos);
        return Value(n->num);
    case Node::Type::Var:
        return ctx.getVar(n->op, n->pos);
    case Node::Type::Const: {
        const SciConstant *c = findConstant(n->op);
        if (!c) throw CalcError(ErrKind::Syntax, n->pos);
        return Value(c->value);
    }
    case Node::Type::Cell:
        if (!ctx.cellHook) throw CalcError(ErrKind::Syntax, n->pos);
        return ctx.cellHook(n->op);
    case Node::Type::Range:
        if (!ctx.rangeHook) throw CalcError(ErrKind::Syntax, n->pos);
        return ctx.rangeHook(n->op);
    case Node::Type::Unary: {
        Value v = eval(n->kids[0]);
        return n->op == "+" ? v : valNeg(v);
    }
    case Node::Type::Binary:
        return check(evalBinary(n), n->pos);
    case Node::Type::Postfix:
        return check(evalPostfix(n), n->pos);
    case Node::Type::Call:
        return check(evalCall(n), n->pos);
    case Node::Type::Paren:
    case Node::Type::Group:
        return eval(n->kids[0]);
    case Node::Type::Assign: {
        Value v = eval(n->kids[0]);
        const QString &t = n->op;
        if (t.startsWith("Mat") && !v.isMatrix()) throw CalcError(ErrKind::Dimension, n->pos);
        if (t.startsWith("Vct") && !v.isVector()) throw CalcError(ErrKind::Dimension, n->pos);
        if (scalarVarNames().contains(t) && !v.isScalar()) throw CalcError(ErrKind::Dimension, n->pos);
        ctx.setVar(t, v);
        return v;
    }
    }
    throw CalcError(ErrKind::Syntax, n->pos);
}

Value Evaluator::evalBinary(const NodePtr &n)
{
    const QString &op = n->op;
    if (op == "=" || op == "<" || op == ">" || op == "≤" || op == "≥" ||
        op == "and" || op == "or" || op == "xor" || op == "xnor")
        throw CalcError(ErrKind::Syntax, n->pos);

    Value a = eval(n->kids[0]);
    Value b = eval(n->kids[1]);
    int pos = n->pos;

    if (op == "+") return valAdd(a, b);
    if (op == "−") return valSub(a, b);
    if (op == "×" || op == "·") return valMul(a, b);
    if (op == "÷" || op == "/") {
        if (b.isScalar() && b.z == cplx(0, 0)) throw CalcError(ErrKind::Math, pos);
        return valDiv(a, b);
    }
    if (op == "^") return power(a, b, pos);
    if (op == "nPr") return Value(num::permutations(real(a, pos), real(b, pos)));
    if (op == "nCr") return Value(num::combinations(real(a, pos), real(b, pos)));
    if (op == "∠") {
        double r = real(a, pos), th = real(b, pos);
        return Value(cplx(r * cosU(th), r * sinU(th)));
    }
    if (op == "÷R") {
        double x = real(a, pos), y = real(b, pos);
        if (!num::isInteger(x) || !num::isInteger(y) || x < 0 || y <= 0 || x >= 1e10 || y >= 1e10)
            throw CalcError(ErrKind::Math, pos);
        double q = std::floor(x / y);
        double r = x - q * y;
        ctx.outputs = {{"Q", Value(q)}, {"R", Value(r)}};
        return Value(q);
    }
    throw CalcError(ErrKind::Syntax, pos);
}

Value Evaluator::evalPostfix(const NodePtr &n)
{
    Value a = eval(n->kids[0]);
    const QString &op = n->op;
    int pos = n->pos;
    if (op == "!") return Value(num::factorial(real(a, pos)));
    if (op == "²") return a.isScalar() ? Value(a.z * a.z) : valMul(a, a);
    if (op == "³") return a.isScalar() ? Value(a.z * a.z * a.z) : valMul(valMul(a, a), a);
    if (op == "⁻¹") {
        if (a.isMatrix()) return matInverse(a);
        cplx z = scalar(a, pos);
        if (z == cplx(0, 0)) throw CalcError(ErrKind::Math, pos);
        return Value(cplx(1, 0) / z);
    }
    if (op == "%") return valDiv(a, Value(100.0));
    if (op == "°" || op == "ʳ" || op == "ᵍ") {
        double v = real(a, pos);
        double rad = op == "°" ? v * PI / 180.0 : op == "ᵍ" ? v * PI / 200.0 : v;
        if (op == "°" && ctx.settings.angle == AngleUnit::Deg) return Value(v);
        if (op == "ᵍ" && ctx.settings.angle == AngleUnit::Gra) return Value(v);
        if (op == "ʳ" && ctx.settings.angle == AngleUnit::Rad) return Value(v);
        return Value(fromRad(rad));
    }
    if (op == QStringLiteral("▶r∠θ") || op == QStringLiteral("▶a+bi")) {
        if (!ctx.complexMode) throw CalcError(ErrKind::Syntax, pos);
        ctx.forcedForm = op.endsWith("bi") ? ForcedForm::Rect : ForcedForm::Polar;
        return a;
    }
    if (op == "conv") {
        const UnitConversion *c = findConversion(n->text);
        if (!c) throw CalcError(ErrKind::Syntax, pos);
        return Value(real(a, pos) * c->factor + c->offset);
    }
    throw CalcError(ErrKind::Syntax, pos);
}

Value Evaluator::evalCalculus(const NodePtr &n)
{
    const QString &f = n->op;
    int pos = n->pos;
    const auto &k = n->kids;
    if (f == QStringLiteral("∫")) {
        if (k.size() != 3) throw CalcError(ErrKind::Argument, pos);
        double a = real(eval(k[1]), pos), b = real(eval(k[2]), pos);
        NodePtr body = k[0];
        return Value(num::integrate([&](double t) { return evalRealAt(body, "x", t); }, a, b));
    }
    if (f == "d/dx") {
        if (k.size() != 2) throw CalcError(ErrKind::Argument, pos);
        double a = real(eval(k[1]), pos);
        NodePtr body = k[0];
        return Value(num::differentiate([&](double t) { return evalRealAt(body, "x", t); }, a));
    }
    // Σ and Π
    if (k.size() != 3) throw CalcError(ErrKind::Argument, pos);
    double a = real(eval(k[1]), pos), b = real(eval(k[2]), pos);
    if (!num::isInteger(a) || !num::isInteger(b) || b < a || b - a > 1e6) throw CalcError(ErrKind::Math, pos);
    bool sum = f == QStringLiteral("Σ");
    Value acc(sum ? 0.0 : 1.0);
    VarGuard g(ctx, "x");
    for (double t = a; t <= b; t += 1) {
        ctx.vars["x"] = Value(t);
        Value v = eval(k[0]);
        acc = sum ? valAdd(acc, v) : valMul(acc, v);
        acc = check(acc, pos);
    }
    return acc;
}

Value Evaluator::evalCall(const NodePtr &n)
{
    const QString &f = n->op;
    int pos = n->pos;
    if (f == QStringLiteral("∫") || f == "d/dx" || f == QStringLiteral("Σ") || f == QStringLiteral("Π"))
        return evalCalculus(n);

    std::vector<Value> args;
    args.reserve(n->kids.size());
    for (const auto &k : n->kids) args.push_back(eval(k));
    auto need = [&](size_t c) { if (args.size() != c) throw CalcError(ErrKind::Argument, pos); };
    auto R = [&](size_t i) { return real(args[i], pos); };
    auto Z = [&](size_t i) { return scalar(args[i], pos); };
    const bool cm = ctx.complexMode;

    // --- trigonometric (real arguments, angle unit aware)
    if (f == "sin") { need(1); return Value(sinU(R(0))); }
    if (f == "cos") { need(1); return Value(cosU(R(0))); }
    if (f == "tan") {
        need(1);
        double c = cosU(R(0));
        if (c == 0) throw CalcError(ErrKind::Math, pos);
        double s = sinU(R(0));
        if (s == 0) return Value(0.0);
        if (std::abs(std::abs(s) - std::abs(c)) < 4e-16) return Value((s > 0) == (c > 0) ? 1.0 : -1.0);
        return Value(std::tan(toRad(R(0))));
    }
    const QString inv = QStringLiteral("⁻¹");
    if (f == "sin" + inv || f == "asin" || f == "cos" + inv || f == "acos") {
        need(1);
        double x = R(0);
        if (x < -1 || x > 1) throw CalcError(ErrKind::Math, pos);
        bool s = f.startsWith("sin") || f == "asin";
        return Value(fromRad(s ? std::asin(x) : std::acos(x)));
    }
    if (f == "tan" + inv || f == "atan") { need(1); return Value(fromRad(std::atan(R(0)))); }
    if (f == "sinh") { need(1); return Value(std::sinh(R(0))); }
    if (f == "cosh") { need(1); return Value(std::cosh(R(0))); }
    if (f == "tanh") { need(1); return Value(std::tanh(R(0))); }
    if (f == "sinh" + inv || f == "asinh") { need(1); return Value(std::asinh(R(0))); }
    if (f == "cosh" + inv || f == "acosh") {
        need(1);
        if (R(0) < 1) throw CalcError(ErrKind::Math, pos);
        return Value(std::acosh(R(0)));
    }
    if (f == "tanh" + inv || f == "atanh") {
        need(1);
        if (std::abs(R(0)) >= 1) throw CalcError(ErrKind::Math, pos);
        return Value(std::atanh(R(0)));
    }

    // --- logarithms, roots, exponentials
    if (f == "log") {
        if (args.size() == 1) {
            cplx z = Z(0);
            if (z.imag() == 0 && z.real() > 0) {
                double l = std::log10(z.real());
                double rl = std::round(l);
                if (std::abs(l - rl) < 1e-15 && std::pow(10.0, rl) == z.real()) l = rl;
                return Value(l);
            }
            if (!cm || z == cplx(0, 0)) throw CalcError(ErrKind::Math, pos);
            return Value(std::log10(z));
        }
        need(2);
        double b = R(0), x = R(1);
        if (b <= 0 || b == 1 || x <= 0) throw CalcError(ErrKind::Math, pos);
        double r = std::log(x) / std::log(b);
        double rr = std::round(r);
        if (std::abs(r - rr) < 1e-13 && std::abs(std::pow(b, rr) - x) <= 1e-14 * x) r = rr;
        return Value(r);
    }
    if (f == "ln") {
        need(1);
        cplx z = Z(0);
        if (z.imag() == 0 && z.real() > 0) return Value(std::log(z.real()));
        if (!cm || z == cplx(0, 0)) throw CalcError(ErrKind::Math, pos);
        return Value(std::log(z));
    }
    if (f == "exp") { need(1); return Value(args[0].isReal() ? cplx(std::exp(R(0)), 0) : std::exp(Z(0))); }
    if (f == QStringLiteral("√")) {
        need(1);
        cplx z = Z(0);
        if (z.imag() == 0) {
            if (z.real() >= 0) return Value(std::sqrt(z.real()));
            if (!cm) throw CalcError(ErrKind::Math, pos);
            return Value(cplx(0, std::sqrt(-z.real())));
        }
        return Value(std::sqrt(z));
    }
    if (f == QStringLiteral("∛")) {
        need(1);
        cplx z = Z(0);
        if (z.imag() == 0) return Value(std::cbrt(z.real()));
        return Value(std::pow(z, 1.0 / 3.0));
    }
    if (f == "root") {
        need(2);
        double nn = R(0);
        if (nn == 0) throw CalcError(ErrKind::Math, pos);
        cplx z = Z(1);
        if (z.imag() == 0 && z.real() < 0 && num::isInteger(nn) && std::fmod(std::abs(nn), 2.0) == 1.0)
            return Value(-std::pow(-z.real(), 1.0 / nn));
        return Value(powScalar(z, cplx(1.0 / nn, 0), pos));
    }

    // --- misc numeric
    if (f == "Abs") {
        need(1);
        const Value &v = args[0];
        if (v.isScalar()) return Value(std::abs(v.z));
        if (v.isVector()) return Value(vecNorm(v));
        Value r = v;
        for (auto &e : r.m) e = std::abs(e);
        return r;
    }
    if (f == "Pol") {
        need(2);
        double x = R(0), y = R(1);
        double r = std::hypot(x, y);
        double th = (x == 0 && y == 0) ? 0.0 : fromRad(std::atan2(y, x));
        ctx.outputs = {{"r", Value(r)}, {QStringLiteral("θ"), Value(th)}};
        ctx.vars["x"] = Value(r);
        ctx.vars["y"] = Value(th);
        return Value(r);
    }
    if (f == "Rec") {
        need(2);
        double r = R(0), th = R(1);
        double x = r * cosU(th), y = r * sinU(th);
        ctx.outputs = {{"x", Value(x)}, {"y", Value(y)}};
        ctx.vars["x"] = Value(x);
        ctx.vars["y"] = Value(y);
        return Value(x);
    }
    if (f == "Int") { need(1); return Value(std::trunc(R(0))); }
    if (f == "Intg") { need(1); return Value(std::floor(R(0))); }
    if (f == "Rnd") { need(1); return Value(roundToDisplay(R(0), ctx.settings)); }
    if (f == "Ran#") {
        std::uniform_int_distribution<int> d(0, 999);
        return Value(d(ctx.rng) / 1000.0);
    }
    if (f == "RanInt#" || f == "RanInt") {
        need(2);
        double a = R(0), b = R(1);
        if (!num::isInteger(a) || !num::isInteger(b) || a > b || std::abs(a) >= 1e10 || std::abs(b) >= 1e10)
            throw CalcError(ErrKind::Math, pos);
        std::uniform_int_distribution<long long> d(static_cast<long long>(a), static_cast<long long>(b));
        return Value(double(d(ctx.rng)));
    }
    if (f == "GCD" || f == "LCM") {
        if (args.size() < 2) throw CalcError(ErrKind::Argument, pos);
        double acc = R(0);
        for (size_t i = 1; i < args.size(); ++i) acc = f == "GCD" ? num::gcd(acc, R(i)) : num::lcm(acc, R(i));
        return Value(acc);
    }
    if (f == "mixed") {
        need(3);
        double a = R(0), b = R(1), c = R(2);
        if (c == 0) throw CalcError(ErrKind::Math, pos);
        return Value(a < 0 ? a - b / c : a + b / c);
    }

    // --- complex
    if (f == "Conjg") { need(1); return Value(std::conj(Z(0))); }
    if (f == "Arg") {
        need(1);
        cplx z = Z(0);
        if (z == cplx(0, 0)) throw CalcError(ErrKind::Math, pos);
        return Value(fromRad(std::arg(z)));
    }
    if (f == "ReP") { need(1); return Value(Z(0).real()); }
    if (f == "ImP") { need(1); return Value(Z(0).imag()); }

    // --- matrix
    if (f == "Det") { need(1); return Value(matDet(args[0])); }
    if (f == "Trn") { need(1); return matTranspose(args[0]); }
    if (f == "Identity") {
        need(1);
        double k = R(0);
        if (!num::isInteger(k)) throw CalcError(ErrKind::Argument, pos);
        return matIdentity(int(k));
    }
    if (f == "Ref") { need(1); return matRowEchelon(args[0], false); }
    if (f == "Rref") { need(1); return matRowEchelon(args[0], true); }

    // --- vector
    if (f == "DotP") { need(2); return Value(vecDot(args[0], args[1])); }
    if (f == "CrossP") { need(2); return vecCross(args[0], args[1]); }
    if (f == "UnitV") {
        need(1);
        if (!args[0].isVector()) throw CalcError(ErrKind::Dimension, pos);
        double nn = vecNorm(args[0]);
        if (nn == 0) throw CalcError(ErrKind::Math, pos);
        return valDiv(args[0], Value(nn));
    }
    if (f == "Angle") {
        need(2);
        if (!args[0].isVector() || !args[1].isVector()) throw CalcError(ErrKind::Dimension, pos);
        double na = vecNorm(args[0]), nb = vecNorm(args[1]);
        if (na == 0 || nb == 0) throw CalcError(ErrKind::Math, pos);
        double c = std::clamp(vecDot(args[0], args[1]).real() / (na * nb), -1.0, 1.0);
        return Value(fromRad(std::acos(c)));
    }

    // --- list functions (spreadsheet)
    if (f == "Min" || f == "Max" || f == "Mean" || f == "Sum") {
        std::vector<double> xs;
        for (const auto &v : args) {
            if (v.isScalar()) xs.push_back(real(v, pos));
            else for (const auto &e : v.m) {
                if (e.imag() != 0) throw CalcError(ErrKind::Math, pos);
                xs.push_back(e.real());
            }
        }
        if (xs.empty()) throw CalcError(ErrKind::Argument, pos);
        double r = xs[0];
        if (f == "Min") for (double x : xs) r = std::min(r, x);
        else if (f == "Max") for (double x : xs) r = std::max(r, x);
        else {
            r = 0;
            for (double x : xs) r += x;
            if (f == "Mean") r /= double(xs.size());
        }
        return Value(r);
    }

    if (ctx.funcHook) {
        if (auto v = ctx.funcHook(f, args)) return *v;
    }
    throw CalcError(ErrKind::Syntax, pos);
}

// ---------------------------------------------------------------- Base-N (32-bit integer) evaluation

qint64 Evaluator::evalInt(const NodePtr &n)
{
    struct DepthGuard {
        int &d;
        explicit DepthGuard(int &x) : d(x) { if (++d > 150) throw CalcError(ErrKind::Stack); }
        ~DepthGuard() { --d; }
    } dg(depth);

    auto rangeCheck = [&](qint64 v) {
        if (v < std::numeric_limits<qint32>::min() || v > std::numeric_limits<qint32>::max())
            throw CalcError(ErrKind::Math, n->pos);
        return v;
    };
    auto u32 = [](qint64 v) { return quint32(qint32(v)); };
    auto s32 = [](quint32 v) { return qint64(qint32(v)); };

    switch (n->type) {
    case Node::Type::Num:
        if (!num::isInteger(n->num)) throw CalcError(ErrKind::Syntax, n->pos);
        return rangeCheck(qint64(n->num));
    case Node::Type::Var: {
        Value v = ctx.getVar(n->op, n->pos);
        if (!v.isReal()) throw CalcError(ErrKind::Math, n->pos);
        return rangeCheck(qint64(std::trunc(v.re())));
    }
    case Node::Type::Paren:
    case Node::Type::Group:
        return evalInt(n->kids[0]);
    case Node::Type::Unary:
        return rangeCheck(n->op == "+" ? evalInt(n->kids[0]) : -evalInt(n->kids[0]));
    case Node::Type::Call: {
        if (n->kids.size() != 1) throw CalcError(ErrKind::Argument, n->pos);
        qint64 a = evalInt(n->kids[0]);
        if (n->op == "Not") return s32(~u32(a));
        if (n->op == "Neg") return rangeCheck(-a);
        throw CalcError(ErrKind::Syntax, n->pos);
    }
    case Node::Type::Binary: {
        qint64 a = evalInt(n->kids[0]);
        qint64 b = evalInt(n->kids[1]);
        const QString &op = n->op;
        if (op == "+") return rangeCheck(a + b);
        if (op == "−") return rangeCheck(a - b);
        if (op == "×" || op == "·") return rangeCheck(a * b);
        if (op == "÷" || op == "/") {
            if (b == 0) throw CalcError(ErrKind::Math, n->pos);
            return rangeCheck(a / b);
        }
        if (op == "and") return s32(u32(a) & u32(b));
        if (op == "or") return s32(u32(a) | u32(b));
        if (op == "xor") return s32(u32(a) ^ u32(b));
        if (op == "xnor") return s32(~(u32(a) ^ u32(b)));
        throw CalcError(ErrKind::Syntax, n->pos);
    }
    default:
        throw CalcError(ErrKind::Syntax, n->pos);
    }
}
