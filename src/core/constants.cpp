#include "constants.h"

// CODATA 2018 values (as used by the fx-991EX/CEX ClassWiz), numbered 01-47 in this order.
const QVector<SciConstant> &scientificConstants()
{
    static const QVector<SciConstant> list = {
        // Universal
        {"h", "h", "Planck constant", "Universal", 6.62607015e-34, "J s"},
        {"hbar", "ħ", "Reduced Planck constant", "Universal", 1.054571817e-34, "J s"},
        {"c0", "c₀", "Speed of light in vacuum", "Universal", 299792458.0, "m s⁻¹"},
        {"eps0", "ε₀", "Electric constant", "Universal", 8.8541878128e-12, "F m⁻¹"},
        {"mu0", "μ₀", "Magnetic constant", "Universal", 1.25663706212e-6, "N A⁻²"},
        {"Z0", "Z₀", "Characteristic impedance of vacuum", "Universal", 376.730313668, "Ω"},
        {"G", "G", "Newtonian constant of gravitation", "Universal", 6.67430e-11, "m³ kg⁻¹ s⁻²"},
        {"lP", "lₚ", "Planck length", "Universal", 1.616255e-35, "m"},
        {"tP", "tₚ", "Planck time", "Universal", 5.391247e-44, "s"},
        // Electromagnetic
        {"muN", "μₙ", "Nuclear magneton", "Electromagnetic", 5.0507837461e-27, "J T⁻¹"},
        {"muB", "μʙ", "Bohr magneton", "Electromagnetic", 9.2740100783e-24, "J T⁻¹"},
        {"e", "e", "Elementary charge", "Electromagnetic", 1.602176634e-19, "C"},
        {"Phi0", "Φ₀", "Magnetic flux quantum", "Electromagnetic", 2.067833848e-15, "Wb"},
        {"G0", "G₀", "Conductance quantum", "Electromagnetic", 7.748091729e-5, "S"},
        {"KJ", "Kⱼ", "Josephson constant", "Electromagnetic", 483597.8484e9, "Hz V⁻¹"},
        {"RK", "Rₖ", "Von Klitzing constant", "Electromagnetic", 25812.80745, "Ω"},
        // Atomic & Nuclear
        {"mp", "mₚ", "Proton mass", "Atomic & Nuclear", 1.67262192369e-27, "kg"},
        {"mn", "mₙ", "Neutron mass", "Atomic & Nuclear", 1.67492749804e-27, "kg"},
        {"me", "mₑ", "Electron mass", "Atomic & Nuclear", 9.1093837015e-31, "kg"},
        {"mmu", "mμ", "Muon mass", "Atomic & Nuclear", 1.883531627e-28, "kg"},
        {"a0", "a₀", "Bohr radius", "Atomic & Nuclear", 5.29177210903e-11, "m"},
        {"alpha", "α", "Fine-structure constant", "Atomic & Nuclear", 7.2973525693e-3, ""},
        {"re", "rₑ", "Classical electron radius", "Atomic & Nuclear", 2.8179403262e-15, "m"},
        {"lc", "λc", "Compton wavelength", "Atomic & Nuclear", 2.42631023867e-12, "m"},
        {"gp", "γₚ", "Proton gyromagnetic ratio", "Atomic & Nuclear", 2.6752218744e8, "s⁻¹ T⁻¹"},
        {"lcp", "λcp", "Proton Compton wavelength", "Atomic & Nuclear", 1.32140985539e-15, "m"},
        {"lcn", "λcn", "Neutron Compton wavelength", "Atomic & Nuclear", 1.31959090581e-15, "m"},
        {"Rinf", "R∞", "Rydberg constant", "Atomic & Nuclear", 10973731.568160, "m⁻¹"},
        {"mup", "μₚ", "Proton magnetic moment", "Atomic & Nuclear", 1.41060679736e-26, "J T⁻¹"},
        {"mue", "μₑ", "Electron magnetic moment", "Atomic & Nuclear", -9.2847647043e-24, "J T⁻¹"},
        {"mun", "μₙ", "Neutron magnetic moment", "Atomic & Nuclear", -9.6623651e-27, "J T⁻¹"},
        {"mumu", "μμ", "Muon magnetic moment", "Atomic & Nuclear", -4.49044830e-26, "J T⁻¹"},
        {"mtau", "mτ", "Tau mass", "Atomic & Nuclear", 3.16754e-27, "kg"},
        // Physico-Chemical
        {"u", "u", "Atomic mass constant", "Physico-Chemical", 1.66053906660e-27, "kg"},
        {"F", "F", "Faraday constant", "Physico-Chemical", 96485.33212, "C mol⁻¹"},
        {"NA", "Nₐ", "Avogadro constant", "Physico-Chemical", 6.02214076e23, "mol⁻¹"},
        {"k", "k", "Boltzmann constant", "Physico-Chemical", 1.380649e-23, "J K⁻¹"},
        {"Vm", "Vₘ", "Molar volume of ideal gas (273.15 K, 101.325 kPa)", "Physico-Chemical", 22.41396954e-3, "m³ mol⁻¹"},
        {"R", "R", "Molar gas constant", "Physico-Chemical", 8.314462618, "J mol⁻¹ K⁻¹"},
        {"c1", "c₁", "First radiation constant", "Physico-Chemical", 3.741771852e-16, "W m²"},
        {"c2", "c₂", "Second radiation constant", "Physico-Chemical", 1.438776877e-2, "m K"},
        {"sigma", "σ", "Stefan-Boltzmann constant", "Physico-Chemical", 5.670374419e-8, "W m⁻² K⁻⁴"},
        // Adopted values
        {"g", "g", "Standard acceleration of gravity", "Adopted Values", 9.80665, "m s⁻²"},
        {"atm", "atm", "Standard atmosphere", "Adopted Values", 101325.0, "Pa"},
        {"RK90", "Rₖ₋₉₀", "Conventional value of von Klitzing constant", "Adopted Values", 25812.807, "Ω"},
        {"KJ90", "Kⱼ₋₉₀", "Conventional value of Josephson constant", "Adopted Values", 483597.9e9, "Hz V⁻¹"},
        // Other
        {"t", "t", "Celsius temperature (0 °C)", "Other", 273.15, "K"},
    };
    return list;
}

const SciConstant *findConstant(const QString &id)
{
    for (const auto &c : scientificConstants())
        if (c.id == id) return &c;
    return nullptr;
}

const QVector<UnitConversion> &unitConversions()
{
    static const QVector<UnitConversion> list = [] {
        QVector<UnitConversion> l;
        auto pair = [&](const QString &a, const QString &b, const QString &cat, double f) {
            l.push_back({a + QChar(0x25B6) + b, cat, f, 0});
            l.push_back({b + QChar(0x25B6) + a, cat, 1.0 / f, 0});
        };
        pair("in", "cm", "Length", 2.54);
        pair("ft", "m", "Length", 0.3048);
        pair("yd", "m", "Length", 0.9144);
        pair("mile", "km", "Length", 1.609344);
        pair("n mile", "m", "Length", 1852.0);
        pair("pc", "km", "Length", 3.0856775814913673e13);
        pair("acre", "m²", "Area", 4046.8564224);
        pair("gal(US)", "L", "Volume", 3.785411784);
        pair("gal(UK)", "L", "Volume", 4.54609);
        pair("km/h", "m/s", "Velocity", 1.0 / 3.6);
        pair("oz", "g", "Mass", 28.349523125);
        pair("lb", "kg", "Mass", 0.45359237);
        pair("atm", "Pa", "Pressure", 101325.0);
        pair("mmHg", "Pa", "Pressure", 133.322387415);
        pair("kgf/cm²", "Pa", "Pressure", 98066.5);
        pair("lbf/in²", "kPa", "Pressure", 6.894757293168);
        pair("hp", "kW", "Power", 0.745699872);
        pair("kgf•m", "J", "Energy", 9.80665);
        pair("J", "cal", "Energy", 1.0 / 4.184);
        const QString F = QStringLiteral("°F"), C = QStringLiteral("°C");
        l.push_back({F + QChar(0x25B6) + C, "Temperature", 5.0 / 9.0, -160.0 / 9.0});
        l.push_back({C + QChar(0x25B6) + F, "Temperature", 9.0 / 5.0, 32.0});
        return l;
    }();
    return list;
}

const UnitConversion *findConversion(const QString &id)
{
    for (const auto &c : unitConversions())
        if (c.id == id) return &c;
    return nullptr;
}
