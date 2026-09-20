#pragma once

#include <QPainter>
#include <QPoint>
#include <QRect>
#include <QString>

void drawCaption(QPainter &p, const QRect &at, const QString &text, int alpha);
void drawSimpleCloud(QPainter &p, QPointF c, double s, const QColor &fill);
void drawWallFan(QPainter &p, QPoint pos, double on, int phase, int wall);
void drawRoofVent(QPainter &p, QPoint pos, double on, int phase);
