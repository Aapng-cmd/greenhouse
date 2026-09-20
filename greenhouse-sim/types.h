#pragma once

#include <QColor>
#include <QPointF>
#include <QString>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct Fly {
    QPointF a, b;
    double u = 0.0;
    QColor color = QColor(40, 90, 200);
};

enum class PipeKind { Inbound, Outbound, Combined };

inline QString groupTitle(int src)
{
    if (src == 1)
        return QString::fromUtf8("темп");
    if (src == 2)
        return QString::fromUtf8("влажн");
    if (src == 3)
        return QString::fromUtf8("свет");
    if (src == 4)
        return QString::fromUtf8("вредит");
    return QString("И%1").arg(src);
}

inline QColor sourceColor(int src)
{
    if (src == 2)
        return QColor(70, 120, 190);
    if (src == 3)
        return QColor(210, 170, 40);
    if (src == 4)
        return QColor(90, 120, 50);
    return QColor(200, 80, 70);
}
