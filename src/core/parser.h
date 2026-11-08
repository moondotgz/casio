#pragma once
#include "lexer.h"
#include <memory>
#include <vector>

struct Node;
using NodePtr = std::shared_ptr<Node>;

struct Node {
    enum class Type {
        Num, Var, Const, Cell, Range,
        Unary,     // op: "−" "+"
        Binary,    // op: + − × ÷ / · ^ nPr nCr ∠ and or xor xnor ÷R = < > ≤ ≥
        Postfix,   // op: ! ² ³ ⁻¹ % ° ʳ ᵍ ▶r∠θ ▶a+bi conv
        Call,      // name + args
        Paren,     // explicit ( ) — kept for rendering
        Group,     // invisible { } grouping
        Assign     // kids[0] → name
    };
    Type type;
    QString op;        // operator / function / variable name
    QString text;      // original literal text (numbers)
    double num = 0;
    int pos = 0;
    bool closed = true;   // for Paren/Call: whether the ')' was present
    std::vector<NodePtr> kids;

    Node(Type t, int p) : type(t), pos(p) {}
};

struct ParseOptions {
    LexOptions lex;
    bool allowRelations = false;   // top-level  =  <  >  ≤  ≥
};

// Parse a single statement. Throws CalcError(Syntax) on failure.
NodePtr parseExpression(const QString &src, const ParseOptions &opt = {});

// Split a multi-statement line at top-level ':' and parse each one.
std::vector<NodePtr> parseStatements(const QString &src, const ParseOptions &opt = {});

// Collect the variable names (A-F, M, x, y) referenced in a tree.
QStringList referencedVariables(const NodePtr &n);
