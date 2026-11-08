#pragma once
#include "value.h"
#include <QtGlobal>
#include <functional>
#include <utility>
#include <vector>

using RealFn = std::function<double(double)>;

namespace num {

double integrate(const RealFn &f, double a, double b);
double differentiate(const RealFn &f, double x);

struct SolveResult { double x; double lr; };
// Find a root of g near x0. Throws CalcError(CantSolve).
SolveResult solve(const RealFn &g, double x0);

// Roots of a polynomial given highest-degree coefficient first (degree 1..4).
std::vector<cplx> polyRoots(const std::vector<double> &coeffs);

enum class LinStatus { Unique, NoSolution, Infinite };
LinStatus solveLinear(std::vector<std::vector<double>> A, std::vector<double> b, std::vector<double> &x);

double normalPdf(double x, double sigma, double mu);
double normalCdf(double lower, double upper, double sigma, double mu);
double invNormalStd(double p);   // standard normal quantile
double binomialPdf(double x, double n, double p);
double binomialCdf(double x, double n, double p);
double poissonPdf(double x, double lambda);
double poissonCdf(double x, double lambda);

double factorial(double n);
double permutations(double n, double r);
double combinations(double n, double r);
double gcd(double a, double b);
double lcm(double a, double b);

// Prime factorisation; factors > maxPrime are left in `rest` (shown in parentheses on the calculator).
std::vector<std::pair<qint64, int>> primeFactors(qint64 n, qint64 &rest);

bool isInteger(double x);
// Try to express y as p/q with q <= maxQ.
bool smallRational(double y, long long &p, long long &q, long long maxQ = 99, double tol = 1e-12);

} // namespace num
