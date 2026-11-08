#pragma once
#include "evaluator.h"
#include "settings.h"
#include "value.h"
#include <QString>

// How a particular result should be shown (toggled by S⇔D, ENG, °'", a b/c⇔d/c ...).
struct DisplayOpts {
    bool approx = false;      // force decimal
    bool dms = false;         // sexagesimal
    bool eng = false;         // engineering notation
    int engShift = 0;
    bool swapFraction = false;   // toggle a b/c ⇔ d/c
    ForcedForm form = ForcedForm::None;
};

// All functions return strings in calculator syntax (parseable, used by the renderer):
//  "−1.5ᴇ−5", "3/4", "{3√(2)}/4" ...
QString formatDecimal(double x, const Settings &s);
QString formatEng(double x, const Settings &s, int shift);
QString formatDMS(double x);
QString exactReal(double x, FracFormat frac);   // empty if no exact form
QString formatReal(double x, const Settings &s, const DisplayOpts &o = {});
QString formatComplex(cplx z, const Settings &s, const DisplayOpts &o = {});
QString formatBaseN(qint64 v, int base);

// Linear, human-readable (no {} grouping) version of a syntax string, for Line I/O and copying.
QString toLinearText(const QString &syntax);

// Plain-text rendering for table cells: exponents as superscripts ("1.5×10⁻⁵").
QString toDisplayText(const QString &syntax);

// True if the value has an exact (non-decimal) representation that differs from the decimal one.
bool hasExactForm(const Value &v, const Settings &s);
