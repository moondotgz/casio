# SciCalc 991

A desktop scientific calculator in C++20 / Qt 6, modelled on the feature set of the
Casio fx-991CEX (ClassWiz): the same 12 calculation modes, a SHIFT/ALPHA keypad and a
"natural display" that shows fractions, radicals, powers, ∫, Σ and matrices in textbook form.

## Build

```sh
cmake -S . -B build -G Ninja
cmake --build build
./build/scicalc991          # the calculator
./build/test_core           # core engine tests
```

You need Qt 6 (Core, Gui, Widgets) and a C++20 compiler.

## Modes (MENU)

| Key | Mode | What it does |
|---|---|---|
| 1 | Calculate | Expressions with exact results (fractions, `a√b/c`, `(a+b√c)/d`, multiples of π), ∫, d/dx, Σ, Π, logₐb, ⁿ√, Pol/Rec, ÷R, nPr/nCr, GCD/LCM, Int/Intg/Rnd, Ran#/RanInt#, °'" (DMS), angle units, 47 scientific constants, 40 unit conversions, CALC, SOLVE, STO/RCL, M+/M−, Ans/PreAns, multi-statement `:` |
| 2 | Complex | `i`, `a+bi` / `r∠θ` input and output, Arg, Conjg, ReP, ImP, Abs, ▶r∠θ, ▶a+bi |
| 3 | Base-N | DEC/HEX/BIN/OCT, 32-bit two's complement, `and or xor xnor Not Neg`, d/h/b/o prefixes |
| 4 | Matrix | MatA–MatD (up to 4×4), MatAns, + − ×, ⁻¹, ², ³, ^n, Det, Trn, Identity, Ref, Rref, Abs |
| 5 | Vector | VctA–VctD (2-D or 3-D), VctAns, DotP, CrossP, Angle, UnitV, Abs |
| 6 | Statistics | 1-variable and 7 regressions (`a+bx`, `a+bx+cx²`, `a+b·ln x`, `a·e^bx`, `a·b^x`, `a·x^b`, `a+b/x`), optional frequency column, quartiles, x̂ / ŷ estimates, statistic-variable calculation line |
| 7 | Distribution | Normal PD/CD, Inverse Normal (left/right/center tail), Binomial PD/CD, Poisson PD/CD, Variable or List input |
| 8 | Spreadsheet | 5×45 cells, `=formulas`, `$` absolute references, Min/Max/Mean/Sum over ranges, Fill Formula, Fill Value, auto/manual recalc, circular-reference detection |
| 9 | Table | f(x) and optional g(x) over Start/End/Step (45 rows, or 30 with g); editing an x cell recomputes its row |
| A | Equation/Func | Simultaneous linear equations with 2–4 unknowns (reports no/infinite solutions); polynomials of degree 2–4 with complex roots, plus the vertex for quadratics |
| B | Inequality | Polynomial inequalities of degree 2–4 with `> < ≥ ≤` |
| C | Ratio | `A:B = X:D` and `A:B = C:X` |

## Using it

- **Keypad**: click the keys. SHIFT (amber legends) and ALPHA (pink legends) latch for one key press.
  OPTN opens the options for the current mode, SHIFT+OPTN opens a catalog of every function, and
  SHIFT 7 / SHIFT 8 open the constants and conversions.
- **Keyboard**: type straight into the input line. `*`, `/`, `-` and `^` all work.
  - Enter: `=`
  - Ctrl+Enter: `≈` (decimal result)
  - Esc: AC
  - ↑/↓: replay earlier calculations
  - F1: MENU, F2: SETUP, F3: OPTN, F4: CATALOG, F5: S⇔D, F6: CALC, F7: SOLVE, F8: Customize
  - Ctrl+T: next theme, Ctrl+K: show/hide keypad
- **After a result**: an operator key continues from `Ans`, and any other key starts a new calculation (as on the real calculator).
  - S⇔D switches between the exact and decimal result.
  - ENG / ←ENG step through engineering notation.
  - °'" converts the result to degrees-minutes-seconds.
  - SHIFT °'" (FACT) shows the prime factorisation.
- **STO**: press STO, then a variable key: A–F on `(−) °'" x⁻¹ sin cos tan`, x on `)`, y on `S⇔D`, M on `M+`. RCL lists every variable.
- **SETUP** has Math/Line I/O, Deg/Rad/Gra, Norm1/Norm2/Fix/Sci, the fraction format, the complex format, statistics frequency, spreadsheet auto-calc, decimal mark and digit separators. Settings, variables, matrices and vectors are saved between sessions.

## Themes & customization

Open the **Customize** dialog from the ⚙ button in the header, **F8**, or **ALPHA MENU** (the THEME legend).
Every change previews live, and Cancel restores what you had before.

- **Theme**: 10 built-in themes (Classic, Midnight, Retro LCD, Ocean, Rosé, Nord, Solarized, Paper,
  Amber Terminal, High Contrast). You can recolour any of the 27 colour roles (body, display, each key
  type, SHIFT/ALPHA legends, cursor pad). **Save as…** keeps your own themes (marked ★), and
  **Import / Export** shares them as JSON files. Ctrl+T / Ctrl+Shift+T switch to the next or previous theme.
- **Display**: interface font and size (keys, input line, menus, tables, dialogs), math font and size for the rendered expressions, show or hide the calculation history.
- **Keypad**: key label size, corner radius, glossy or flat keys, SHIFT/ALPHA legends on or off,
  template symbols as letters (a/b, √x, xʸ, 10ˣ — the default) or Casio-style slots (□/□, √□), and
  hiding the keypad for a compact display-only window (**Ctrl+K**).
- **Window**: calculator name, model and tagline, always-on-top, and window opacity.

The dialogs and menus follow the active theme too. Everything is saved with your other settings.

Fonts without superscript glyphs are fine: key labels, table cells and Line-mode results draw their own
superscripts, subscripts and □ template boxes, and menus fall back to `x^2`-style spellings only for the
characters your font lacks. `--font "<family>"` starts the app with a different interface font.

## Architecture

```
src/core/      UI-independent engine (QtCore only, covered by tests/test_core.cpp)
  lexer        tokens, incl. ᴇ exponents, D°M'S" literals, ‹constants›, Base-N digits, cell refs
  parser       Pratt-style precedence parser → AST (implicit ×, postfix ops, ':' statements)
  evaluator    Value = complex scalar | matrix | vector; angle-aware trig with exact quadrant values
  numeric      Gauss–Kronrod ∫, Ridders d/dx, Newton+bracketing SOLVE, polynomial roots
               (exact rational deflation + closed-form quadratic), linear systems, distributions
  format       Norm/Fix/Sci/ENG/DMS formatting; exact-form recognition (fractions, radicals, π)
  stats, constants
src/gui/       Qt Widgets: keypad, box-layout math renderer (MathView), one page per mode,
               theme.* (colour roles, presets, persistence) and customizedialog.*
```

## Differences from the real calculator

- Arithmetic uses IEEE doubles (about 15–16 significant digits) and shows 10. Exact forms come
  from recognising a numeric result as a fraction, radical or multiple of π, so results that are
  only approximately exact (for example from numeric ∫ or d/dx) appear as decimals.
- Input is typed on a single line, and the natural display shows the rendered expression above it.
  The real calculator edits inside 2-D templates instead.
- Not included: QR-code output, engineering-symbol input (k, M, m, μ …) and the contrast setting.
- Π (product) and Ref/Rref are available as extras.
