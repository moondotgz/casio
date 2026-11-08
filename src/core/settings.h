#pragma once

enum class AngleUnit { Deg, Rad, Gra };
enum class NumFormat { Norm1, Norm2, Fix, Sci };
enum class IOMode { MathMath, MathDec, LineLine, LineDec };
enum class FracFormat { Improper, Mixed };   // d/c  vs  a b/c
enum class ComplexForm { Rect, Polar };      // a+bi vs r∠θ

struct Settings {
    AngleUnit angle = AngleUnit::Deg;
    NumFormat format = NumFormat::Norm1;
    int digits = 0;                 // Fix: 0-9 decimals, Sci: 0-9 sig digits (0 = 10)
    IOMode io = IOMode::MathMath;
    FracFormat frac = FracFormat::Improper;
    ComplexForm complexForm = ComplexForm::Rect;
    bool statFrequency = false;
    bool sheetAutoCalc = true;
    bool decimalComma = false;
    bool digitSeparator = false;
    bool tableUseG = true;

    bool exactOutput() const { return io == IOMode::MathMath || io == IOMode::LineLine; }
    bool naturalInput() const { return io == IOMode::MathMath || io == IOMode::MathDec; }
};
