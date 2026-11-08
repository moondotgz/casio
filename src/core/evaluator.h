#pragma once
#include "error.h"
#include "parser.h"
#include "settings.h"
#include "value.h"
#include <QList>
#include <QMap>
#include <QPair>
#include <functional>
#include <optional>
#include <random>

enum class ForcedForm { None, Rect, Polar };

// Shared calculator state: variables, settings, mode flags and hooks.
struct Context {
    Settings settings;
    QMap<QString, Value> vars;

    bool complexMode = false;
    int base = 0;   // Base-N mode when non-zero

    // Mode-specific extensions (statistics variables, spreadsheet cells ...)
    std::function<std::optional<Value>(const QString &)> varHook;
    std::function<std::optional<Value>(const QString &, const std::vector<Value> &)> funcHook;
    std::function<Value(const QString &)> cellHook;    // "A1"
    std::function<Value(const QString &)> rangeHook;   // "A1:B3" -> vector

    // Outputs produced by Pol / Rec / ÷R and display form requests
    QList<QPair<QString, Value>> outputs;
    ForcedForm forcedForm = ForcedForm::None;

    std::mt19937_64 rng{std::random_device{}()};

    Context();
    Value getVar(const QString &name, int pos = -1) const;
    void setVar(const QString &name, const Value &v);
    void pushAns(const Value &v);   // Ans -> PreAns, v -> Ans (scalar)
};

class Evaluator {
public:
    explicit Evaluator(Context &c) : ctx(c) {}

    // Evaluate a whole statement (handles assignment, resets outputs).
    Value evaluate(const NodePtr &n);
    // Evaluate any sub-expression.
    Value eval(const NodePtr &n);

    // Evaluate the tree as a real function of x (used by numeric calculus, tables, SOLVE).
    double evalRealAt(const NodePtr &n, const QString &var, double v);

    // Angle helpers (respecting the current angle unit)
    double toRad(double a) const;
    double fromRad(double r) const;

private:
    Context &ctx;
    int depth = 0;

    Value evalBinary(const NodePtr &n);
    Value evalPostfix(const NodePtr &n);
    Value evalCall(const NodePtr &n);
    Value evalCalculus(const NodePtr &n);
    qint64 evalInt(const NodePtr &n);

    cplx scalar(const Value &v, int pos) const;
    double real(const Value &v, int pos) const;
    Value check(const Value &v, int pos) const;
    cplx powScalar(cplx a, cplx b, int pos) const;
    Value power(const Value &a, const Value &b, int pos) const;

    double sinU(double a) const;
    double cosU(double a) const;
};

// Round a real number the way the calculator's Rnd function does under the current display format.
double roundToDisplay(double x, const Settings &s);
