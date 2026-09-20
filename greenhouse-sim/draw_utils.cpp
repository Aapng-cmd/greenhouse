#include "draw_utils.h"
#include "types.h"

#include <cmath>

void drawCaption(QPainter &p, const QRect &at, const QString &text, int alpha)
{
    p.setPen(QColor(25, 35, 25, qBound(70, alpha, 230)));
    p.setFont(QFont("Sans", 7, QFont::DemiBold));
    p.drawText(at, Qt::AlignCenter, text);
}

void drawSimpleCloud(QPainter &p, QPointF c, double s, const QColor &fill)
{
    p.setPen(Qt::NoPen);
    p.setBrush(fill);
    p.drawEllipse(c + QPointF(-0.22 * s, 0.04 * s), 0.42 * s, 0.24 * s);
    p.drawEllipse(c + QPointF(0.18 * s, -0.04 * s), 0.46 * s, 0.28 * s);
}

void drawWallFan(QPainter &p, QPoint pos, double on, int phase, int wall)
{
    const QPoint fanC = pos;
    p.setPen(QPen(QColor(50, 90, 140), 2));
    p.setBrush(QColor(170, 200, 230, 220));
    p.drawRoundedRect(QRect(fanC.x() - 16, fanC.y() - 16, 32, 32), 4, 4);
    p.setPen(QPen(QColor(40, 70, 110), 1));
    p.setBrush(QColor(210, 230, 245));
    p.drawEllipse(fanC, 12, 12);
    p.setBrush(QColor(30, 90, 170, int(140 + 80 * on)));
    for (int b = 0; b < 3; ++b) {
        const double a = (phase * (4.0 + 14.0 * on) + b * 120.0) * M_PI / 180.0;
        QPolygonF blade;
        blade << fanC
              << QPointF(fanC.x() + 11 * std::cos(a - 0.35), fanC.y() + 11 * std::sin(a - 0.35))
              << QPointF(fanC.x() + 12 * std::cos(a), fanC.y() + 12 * std::sin(a))
              << QPointF(fanC.x() + 11 * std::cos(a + 0.35), fanC.y() + 11 * std::sin(a + 0.35));
        p.drawPolygon(blade);
    }
    p.setBrush(QColor(20, 40, 70));
    p.setPen(Qt::NoPen);
    p.drawEllipse(fanC, 3, 3);
    if (on > 0.05) {
        const int dir = (wall == 1) ? 1 : -1;
        p.setPen(QPen(QColor(90, 150, 220, int(50 + 140 * on)), 2));
        for (int i = 0; i < 3; ++i)
            p.drawLine(fanC.x() + dir * (18 + int(10 * on)), fanC.y() - 8 + i * 8,
                       fanC.x() + dir * (28 + int(16 * on)), fanC.y() - 4 + i * 8);
    }
}

void drawRoofVent(QPainter &p, QPoint pos, double on, int phase)
{
    const int x = pos.x();
    const int y = pos.y();
    p.setPen(QPen(QColor(70, 50, 40), 1));
    p.setBrush(QColor(130, 90, 70));
    p.drawRect(x - 11, y + 8, 22, 6);
    p.setBrush(QColor(88, 92, 96));
    p.setPen(QPen(QColor(40, 44, 48), 1));
    p.drawRect(x - 5, y - 16, 10, 26);
    QPolygon hood;
    hood << QPoint(x - 15, y - 22) << QPoint(x + 15, y - 22)
         << QPoint(x + 7, y - 8) << QPoint(x - 7, y - 8);
    p.setBrush(QColor(62, 68, 74));
    p.drawPolygon(hood);
    const QPoint cap(x, y - 26);
    p.setBrush(QColor(150, 158, 148));
    p.setPen(QPen(QColor(70, 80, 70), 1));
    p.drawEllipse(cap, 9, 5);
    p.setPen(QPen(QColor(40, 55, 45, int(150 + 80 * on)), 2));
    for (int b = 0; b < 4; ++b) {
        const double a = (phase * (2.0 + 9.0 * on) + b * 45.0) * M_PI / 180.0;
        p.drawLine(QPointF(cap.x() + 8 * std::cos(a), cap.y() + 3 * std::sin(a)),
                   QPointF(cap.x() - 8 * std::cos(a), cap.y() - 3 * std::sin(a)));
    }
    if (on > 0.05) {
        p.setPen(QPen(QColor(180, 210, 215, int(50 + 130 * on)), 2));
        for (int i = 0; i < 3; ++i) {
            const int wobble = int(4 * std::sin((phase + i * 35) * 0.12));
            p.drawArc(QRect(x - 9 + wobble, y - 46 - i * 8, 18, 12), 0, 180 * 16);
        }
    }
}
