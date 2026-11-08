#pragma once
#include <QString>
#include <QStringList>
#include <QVector>
#include <map>
#include <optional>
#include <vector>

enum class RegType { OneVar, Linear, Quadratic, Log, ExpE, ExpAB, Power, Inverse };

QString regTypeName(RegType t);     // "y=a+bx" ...
bool regIsPaired(RegType t);

struct StatData {
    std::vector<double> x, y, f;    // f empty => all frequencies 1
};

struct StatResult {
    RegType type = RegType::OneVar;
    std::map<QString, double> v;    // keyed by stat variable name (n, x̄, σx, a, b, r ...)
    QStringList oneVarOrder, pairedOrder, regOrder;
    bool quartiles = false;

    std::optional<double> get(const QString &k) const;
    double estimateY(double x) const;                 // ŷ
    std::vector<double> estimateX(double y) const;    // x̂ (two values for quadratic)
};

// Throws CalcError on invalid data.
StatResult computeStats(const StatData &d, RegType t);

// Names usable as variables in the statistics calculation line.
QStringList statVariableNames();
