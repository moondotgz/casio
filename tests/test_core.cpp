#include "core/evaluator.h"
#include "core/format.h"
#include "core/numeric.h"
#include "core/stats.h"
#include <QCoreApplication>
#include <cmath>
#include <cstdio>

static int failures = 0, passes = 0;

static QString run(Context &ctx, const QString &src, bool approx = false)
{
    try {
        ParseOptions po;
        po.lex.base = ctx.base;
        auto stmts = parseStatements(src, po);
        Evaluator ev(ctx);
        Value v;
        for (auto &s : stmts) { v = ev.evaluate(s); ctx.pushAns(v); }
        if (ctx.base) return formatBaseN(qint64(v.re()), ctx.base);
        DisplayOpts o;
        o.approx = approx;
        o.form = ctx.forcedForm;
        if (v.isScalar()) return formatComplex(v.z, ctx.settings, o);
        QStringList parts;
        for (auto &e : v.m) parts << formatComplex(e, ctx.settings, o);
        return "[" + parts.join(",") + "]";
    } catch (const CalcError &e) {
        return errorName(e.kind);
    }
}

static void expect(Context &ctx, const QString &src, const QString &want, bool approx = false)
{
    QString got = run(ctx, src, approx);
    if (got == want) {
        ++passes;
    } else {
        ++failures;
        std::printf("FAIL  %-34s got %-24s want %s\n", qPrintable(src), qPrintable(got), qPrintable(want));
    }
}

int main(int argc, char **argv)
{
    QCoreApplication app(argc, argv);
    const QString M = QStringLiteral("−");
    {
        Context c;
        expect(c, "1+2×3", "7");
        expect(c, "1/3+1/6", "1/2");
        expect(c, "2^10", "1024");
        expect(c, "−2²", M + "4");
        expect(c, "(−2)²", "4");
        expect(c, "√(8)", "2√(2)");
        expect(c, "√(2)/2", "√(2)/2");
        expect(c, "sin(30)", "1/2");
        expect(c, "sin(60)", "√(3)/2");
        expect(c, "cos(90)", "0");
        expect(c, "tan(45)", "1");
        expect(c, "tan(90)", "Math ERROR");
        expect(c, "sin⁻¹(1)", "90");
        expect(c, "1÷0", "Math ERROR");
        expect(c, "√(−1)", "Math ERROR");
        expect(c, "5!", "120");
        expect(c, "10nCr3", "120");
        expect(c, "5nPr2", "20");
        expect(c, "log(100)", "2");
        expect(c, "log(2,8)", "3");
        expect(c, "ln(e)", "1");
        expect(c, "2π", "2π");
        expect(c, "π/3", "π/3");
        expect(c, "1÷2π", formatReal(1 / (2 * M_PI), c.settings));
        expect(c, "(1+√(5))/2", "{1+√(5)}/2");
        expect(c, "0.1+0.2", "3/10");
        expect(c, "0.1+0.2", "0.3", true);
        expect(c, "1/3", "0.3333333333", true);
        expect(c, "123456789×1000", "1.23456789ᴇ11");
        expect(c, "12345678×100", "1234567800");
        expect(c, "2ᴇ10×3", "6ᴇ10");
        expect(c, "1ᴇ−5", "1/100000");
        expect(c, "1ᴇ−5", "1ᴇ" + M + "5", true);
        expect(c, "∛(−8)", M + "2");
        expect(c, "(−8)^(1/3)", M + "2");
        expect(c, "root(3,27)", "3");
        expect(c, "GCD(12,18)", "6");
        expect(c, "LCM(4,6)", "12");
        expect(c, "Int(−2.5)", M + "2");
        expect(c, "Intg(−2.5)", M + "3");
        expect(c, "∫(x²,0,3)", "9");
        expect(c, "d/dx(x³,2)", "12");
        expect(c, "Σ(x,1,100)", "5050");
        expect(c, "Π(x,1,5)", "120");
        expect(c, "5→A", "5");
        expect(c, "A²+1", "26");
        expect(c, "2A", "10");
        expect(c, "Ans×2", "20");
        expect(c, "3:4:Ans+PreAns", "7");
        expect(c, "17÷R5", "3");
        expect(c, "Pol(1,1)", "√(2)");
        expect(c, "y", "45");
        expect(c, "Rec(2,30)", "√(3)");
        expect(c, "30°15'", "121/4");
        expect(c, "1⌟1⌟2+1/2", "2");
        expect(c, "‹c0›", "299792458");
        expect(c, "1‹in▶cm›", "127/50");
        expect(c, "212‹°F▶°C›", "100");
        expect(c, "50%", "1/2");
        expect(c, "90ʳ", formatReal(90 * 180 / M_PI, c.settings));
        expect(c, "2(3+4)", "14");
        expect(c, "(1+2", "3");
        expect(c, "sin(", "Syntax ERROR");
        expect(c, "1++", "Syntax ERROR");
        expect(c, "69!", "1.711224524ᴇ98");
        expect(c, "70!", "Math ERROR");
        expect(c, "Rnd(1/3)", "0.3333333333");
        c.settings.angle = AngleUnit::Rad;
        expect(c, "sin(π/6)", "1/2");
        expect(c, "cos(π)", M + "1");
        expect(c, "sin⁻¹(1)", "π/2");
        expect(c, "90°", "π/2");
        c.settings.angle = AngleUnit::Deg;
        c.settings.format = NumFormat::Fix;
        c.settings.digits = 3;
        expect(c, "2/3", "0.667", true);
        c.settings.format = NumFormat::Sci;
        c.settings.digits = 4;
        expect(c, "1234", "1.234ᴇ3", true);
        c.settings.format = NumFormat::Norm1;
        c.settings.frac = FracFormat::Mixed;
        expect(c, "7/2", "3⌟1⌟2");
        c.settings.frac = FracFormat::Improper;
    }
    {   // statistics variables through hooks, precomposed ŷ accepted
        Context c;
        StatData d;
        d.x = {1, 2, 3, 4};
        d.y = {3, 5, 7.1, 8.9};
        StatResult r = computeStats(d, RegType::Linear);
        c.varHook = [r](const QString &n) -> std::optional<Value> { if (auto v = r.get(n)) return Value(*v); return std::nullopt; };
        c.funcHook = [r](const QString &n, const std::vector<Value> &a) -> std::optional<Value> {
            if (n == QStringLiteral("y\u0302")) return Value(r.estimateY(a[0].re()));
            return std::nullopt;
        };
        ParseOptions po;
        po.lex.extraVars = statVariableNames();
        po.lex.extraFuncs = {QStringLiteral("y\u0302")};
        for (const QString &src : {QStringLiteral("\u0177(10)"), QStringLiteral("y\u0302(10)")}) {
            Evaluator ev(c);
            Value v = ev.evaluate(parseExpression(src, po));
            if (std::abs(v.re() - 20.85) < 1e-9) ++passes; else { ++failures; std::printf("FAIL yhat %g\n", v.re()); }
        }
    }
    {   // complex
        Context c;
        c.complexMode = true;
        expect(c, "i²", M + "1");
        expect(c, "(1+i)(1−i)", "2");
        expect(c, "√(−4)", "{2}i");
        expect(c, "1/(1+i)", "1/2" + M + "{1/2}i");
        expect(c, "Abs(3+4i)", "5");
        expect(c, "Arg(i)", "90");
        expect(c, "Conjg(2+3i)", "2" + M + "{3}i");
        expect(c, "2∠60", "1+{√(3)}i");
        expect(c, "1+i▶r∠θ", "√(2)∠{45}");
    }
    {   // base-n
        Context c;
        c.base = 16;
        expect(c, "FF+1", "100");
        expect(c, "Not(0)", "FFFFFFFF");
        expect(c, "d10+d5", "F");
        c.base = 2;
        expect(c, "1010 and 0110", "10");
        expect(c, "1010 xor 0110", "1100");
        c.base = 10;
        expect(c, "7÷2", "3");
        expect(c, "2147483647+1", "Math ERROR");
        expect(c, "Neg(5)", M + "5");
    }
    {   // matrix / vector
        Context c;
        Value A = Value::matrix(2, 2);
        A.at(0, 0) = 1; A.at(0, 1) = 2; A.at(1, 0) = 3; A.at(1, 1) = 4;
        c.vars["MatA"] = A;
        expect(c, "Det(MatA)", M + "2");
        expect(c, "MatA⁻¹", "[" + M + "2,1,3/2," + M + "{1/2}]");
        expect(c, "MatA×MatA", "[7,10,15,22]");
        expect(c, "Trn(MatA)", "[1,3,2,4]");
        expect(c, "MatA+Identity(2)", "[2,2,3,5]");
        expect(c, "MatA+Identity(3)", "Dimension ERROR");
        Value u = Value::vector(3), v = Value::vector(3);
        u[0] = 1; u[1] = 0; u[2] = 0;
        v[0] = 0; v[1] = 1; v[2] = 0;
        c.vars["VctA"] = u;
        c.vars["VctB"] = v;
        expect(c, "CrossP(VctA,VctB)", "[0,0,1]");
        expect(c, "DotP(VctA,VctB)", "0");
        expect(c, "Angle(VctA,VctB)", "90");
        expect(c, "Abs(VctA+VctB)", "√(2)");
    }
    {   // numeric helpers
        auto r = num::polyRoots({1, -3, 2});
        if (r.size() == 2 && r[0] == cplx(2, 0) && r[1] == cplx(1, 0)) ++passes;
        else { ++failures; std::printf("FAIL quadratic roots\n"); }
        r = num::polyRoots({1, -6, 11, -6});
        if (r.size() == 3 && r[0] == cplx(3, 0) && r[1] == cplx(2, 0) && r[2] == cplx(1, 0)) ++passes;
        else { ++failures; std::printf("FAIL cubic roots\n"); }
        r = num::polyRoots({1, 0, 0, 0, -1});
        if (r.size() == 4 && r[0] == cplx(1, 0) && r[1] == cplx(-1, 0) && std::abs(r[2] - cplx(0, 1)) < 1e-14) ++passes;
        else { ++failures; std::printf("FAIL quartic roots\n"); }
        r = num::polyRoots({1, 0, -2});
        if (exactReal(r[0].real(), FracFormat::Improper) == "√(2)") ++passes;
        else { ++failures; std::printf("FAIL sqrt2 root\n"); }
        std::vector<double> x;
        auto st = num::solveLinear({{1, 1}, {1, -1}}, {3, 1}, x);
        if (st == num::LinStatus::Unique && x[0] == 2 && x[1] == 1) ++passes;
        else { ++failures; std::printf("FAIL linear\n"); }
        st = num::solveLinear({{1, 1}, {2, 2}}, {1, 3}, x);
        if (st == num::LinStatus::NoSolution) ++passes; else { ++failures; std::printf("FAIL nosol\n"); }
        st = num::solveLinear({{1, 1}, {2, 2}}, {1, 2}, x);
        if (st == num::LinStatus::Infinite) ++passes; else { ++failures; std::printf("FAIL inf\n"); }
        auto s = num::solve([](double t) { return t * t - 2; }, 1);
        if (std::abs(s.x - std::sqrt(2.0)) < 1e-14) ++passes; else { ++failures; std::printf("FAIL solve %.17g\n", s.x); }
        s = num::solve([](double t) { return std::exp(t) - 10; }, 100);
        if (std::abs(s.x - std::log(10.0)) < 1e-12) ++passes; else { ++failures; std::printf("FAIL solve2 %.17g\n", s.x); }
        double p = num::normalCdf(-1, 1, 1, 0);
        if (std::abs(p - 0.6826894921) < 1e-9) ++passes; else { ++failures; std::printf("FAIL ncd %.12g\n", p); }
        double z = num::invNormalStd(0.975);
        if (std::abs(z - 1.959963985) < 1e-9) ++passes; else { ++failures; std::printf("FAIL invn %.12g\n", z); }
        double b = num::binomialPdf(3, 10, 0.5);
        if (std::abs(b - 0.1171875) < 1e-15) ++passes; else { ++failures; std::printf("FAIL binom %.12g\n", b); }
        double pc = num::poissonCdf(2, 3);
        if (std::abs(pc - 0.4231900811) < 1e-9) ++passes; else { ++failures; std::printf("FAIL poisson %.12g\n", pc); }
    }
    {   // statistics
        StatData d;
        d.x = {1, 2, 3, 4, 5};
        auto r = computeStats(d, RegType::OneVar);
        if (r.v["n"] == 5 && r.v["x̄"] == 3 && r.v["Med"] == 3 && r.v["Q1"] == 1.5 && r.v["Q3"] == 4.5) ++passes;
        else { ++failures; std::printf("FAIL 1var\n"); }
        d.y = {2, 4, 6, 8, 10};
        r = computeStats(d, RegType::Linear);
        if (std::abs(r.v["a"]) < 1e-12 && std::abs(r.v["b"] - 2) < 1e-12 && std::abs(r.v["r"] - 1) < 1e-12) ++passes;
        else { ++failures; std::printf("FAIL linreg\n"); }
        d.y = {1, 4, 9, 16, 25};
        r = computeStats(d, RegType::Quadratic);
        if (std::abs(r.v["c"] - 1) < 1e-9 && std::abs(r.v["b"]) < 1e-9 && std::abs(r.v["a"]) < 1e-9) ++passes;
        else { ++failures; std::printf("FAIL quadreg %g %g %g\n", r.v["a"], r.v["b"], r.v["c"]); }
    }
    {   // formatting
        Settings s;
        if (formatDMS(12.5) == "12°30'0\"") ++passes; else { ++failures; std::printf("FAIL dms %s\n", qPrintable(formatDMS(12.5))); }
        if (formatEng(1234, s, 0) == "1.234ᴇ3") ++passes; else { ++failures; std::printf("FAIL eng %s\n", qPrintable(formatEng(1234, s, 0))); }
        if (formatEng(1234, s, 1) == "1234ᴇ0") ++passes; else { ++failures; std::printf("FAIL eng1 %s\n", qPrintable(formatEng(1234, s, 1))); }
        if (formatDecimal(0.001, s) == "1ᴇ" + M + "3") ++passes; else { ++failures; std::printf("FAIL norm1\n"); }
        s.format = NumFormat::Norm2;
        if (formatDecimal(0.001, s) == "0.001") ++passes; else { ++failures; std::printf("FAIL norm2\n"); }
    }
    std::printf("%d passed, %d failed\n", passes, failures);
    return failures ? 1 : 0;
}
