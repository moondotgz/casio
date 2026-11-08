#pragma once
#include <QString>
#include <QVector>

struct SciConstant {
    QString id;        // token id used inside ‹ ›
    QString symbol;    // display symbol
    QString name;
    QString category;
    double value;
    QString unit;
};

struct UnitConversion {
    QString id;        // e.g. "in▶cm" (also the token text inside ‹ ›)
    QString category;
    double factor;     // to = from * factor + offset
    double offset = 0;
};

const QVector<SciConstant> &scientificConstants();
const SciConstant *findConstant(const QString &id);

const QVector<UnitConversion> &unitConversions();
const UnitConversion *findConversion(const QString &id);
