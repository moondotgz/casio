#pragma once
#include <QString>

enum class ErrKind {
    Syntax, Math, Stack, Argument, Dimension, CantSolve, Variable, TimeOut, Range, Circular, NoSolution
};

struct CalcError {
    ErrKind kind;
    int pos = -1;          // character offset into the source, -1 if unknown
    QString detail;
    CalcError(ErrKind k, int p = -1, QString d = {}) : kind(k), pos(p), detail(std::move(d)) {}
};

inline QString errorName(ErrKind k)
{
    switch (k) {
    case ErrKind::Syntax: return QStringLiteral("Syntax ERROR");
    case ErrKind::Math: return QStringLiteral("Math ERROR");
    case ErrKind::Stack: return QStringLiteral("Stack ERROR");
    case ErrKind::Argument: return QStringLiteral("Argument ERROR");
    case ErrKind::Dimension: return QStringLiteral("Dimension ERROR");
    case ErrKind::CantSolve: return QStringLiteral("Can't Solve");
    case ErrKind::Variable: return QStringLiteral("Variable ERROR");
    case ErrKind::TimeOut: return QStringLiteral("Time Out");
    case ErrKind::Range: return QStringLiteral("Range ERROR");
    case ErrKind::Circular: return QStringLiteral("Circular ERROR");
    case ErrKind::NoSolution: return QStringLiteral("No Solution");
    }
    return QStringLiteral("ERROR");
}
