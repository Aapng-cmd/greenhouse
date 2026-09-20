#include "greenhouse_view.h"

#include "draw_utils.h"
#include "types.h"

#include <QHash>
#include <QJsonArray>
#include <QPaintEvent>
#include <QPainter>
#include <QRandomGenerator>
#include <QSet>
#include <QTimer>
#include <cmath>

GreenhouseView::GreenhouseView(QWidget *parent) : QWidget(parent)
{
    setMinimumSize(160, 120);
    setAttribute(Qt::WA_OpaquePaintEvent);
    const QStringList keys = {
        QStringLiteral("shade"), QStringLiteral("uv"), QStringLiteral("spray_water"),
        QStringLiteral("chem"), QStringLiteral("heat"), QStringLiteral("cool"),
        QStringLiteral("vent")
    };
    for (const auto &k : keys)
        level[k] = 0.0;
    auto *tick = new QTimer(this);
    connect(tick, &QTimer::timeout, this, [this, keys]() {
        if (!isVisible())
            return;
        bool dirty = false;
        const auto cl = frame.value("climate").toObject();
        const QString action = cl.value("action").toString();
        for (const auto &k : keys) {
            const double target = (action == k) ? 1.0 : 0.0;
            double &v = level[k];
            const double prev = v;
            if (v < target)
                v = qMin(target, v + 0.16);
            else
                v = qMax(target, v - 0.10);
            if (qAbs(v - prev) > 0.0005)
                dirty = true;
        }
        phase = (phase + 1) % 720;
        bool animating = dirty;
        if (!animating) {
            for (const auto &k : keys) {
                if (level.value(k) > 0.02) {
                    animating = true;
                    break;
                }
            }
        }
        if (!animating && cl.value("pests").toDouble() > 3.0)
            animating = true;
        if (animating)
            update();
    });
    tick->start(50);
}

void GreenhouseView::rebuildPlaced(const QRect &house, const QRect &glass)
{
    const auto acts = frame.value("actuators").toArray();
    QString key = QString::number(house.x()) + "," + QString::number(house.y()) + ","
                  + QString::number(house.width()) + "x" + QString::number(house.height());
    for (const auto &v : acts) {
        const auto o = v.toObject();
        key += QString(";%1:%2").arg(o.value("id").toInt()).arg(o.value("name").toString());
    }
    if (key == placedKey)
        return;
    placedKey = key;
    placed.clear();
    QList<int> shade, uv, spray, chem, trap, heat, coolFans, exhausts;
    QHash<int, QString> names;
    QHash<int, QString> actions;
    for (const auto &v : acts) {
        const auto o = v.toObject();
        const int id = o.value("id").toInt();
        const QString action = o.value("action").toString();
        const QString name = o.value("name").toString();
        names.insert(id, name);
        actions.insert(id, action);
        if (action == QLatin1String("shade"))
            shade << id;
        else if (action == QLatin1String("uv"))
            uv << id;
        else if (action == QLatin1String("spray_water"))
            spray << id;
        else if (action == QLatin1String("heat"))
            heat << id;
        else if (action == QLatin1String("cool"))
            coolFans << id;
        else if (action == QLatin1String("vent"))
            exhausts << id;
        else if (action == QLatin1String("chem") && name.contains(QString::fromUtf8("ловуш")))
            trap << id;
        else if (action == QLatin1String("chem"))
            chem << id;
    }
    QRandomGenerator rng(qHash(key));
    auto jitter = [&](int span) { return int(rng.bounded(-span, span + 1)); };
    auto add = [&](int id, const QPoint &pos, int variant) {
        PlacedAct a;
        a.id = id;
        a.action = actions.value(id);
        a.name = names.value(id);
        a.pos = pos;
        a.variant = variant;
        placed.append(a);
    };
    const int peakY = house.top() - 26;
    const int eaveY = house.top() + 40;
    const int midX = house.center().x();
    auto roofAt = [&](int x) {
        if (x <= midX) {
            const double u = double(x - house.left()) / qMax(1, midX - house.left());
            return int((1.0 - u) * eaveY + u * peakY);
        }
        const double u = double(x - midX) / qMax(1, house.right() - midX);
        return int((1.0 - u) * peakY + u * eaveY);
    };
    for (int i = 0; i < shade.size(); ++i) {
        const double t = (shade.size() == 1) ? 0.5 : (0.14 + 0.72 * i / (shade.size() - 1));
        const int x = house.left() + int(t * house.width()) + jitter(6);
        add(shade[i], QPoint(x, roofAt(x) + 10), i);
    }
    for (int i = 0; i < exhausts.size(); ++i) {
        const double t = (exhausts.size() == 1) ? 0.62 : (0.22 + 0.56 * i / (exhausts.size() - 1));
        const int x = house.left() + int(t * house.width()) + jitter(8);
        add(exhausts[i], QPoint(x, roofAt(x) + 2), i);
    }
    for (int i = 0; i < uv.size(); ++i) {
        const double t = (uv.size() == 1) ? 0.5 : (0.12 + 0.76 * i / (uv.size() - 1));
        const int x = glass.left() + int(t * glass.width()) + jitter(4);
        add(uv[i], QPoint(x, glass.top() + 10), i);
    }
    for (int i = 0; i < spray.size(); ++i) {
        const double t = (spray.size() == 1) ? 0.5 : (0.16 + 0.68 * i / (spray.size() - 1));
        const int x = glass.left() + int(t * glass.width()) + jitter(5);
        add(spray[i], QPoint(x, glass.top() + 32), i);
    }
    for (int i = 0; i < chem.size(); ++i) {
        const double t = (chem.size() == 1) ? 0.5 : (0.22 + 0.56 * i / (chem.size() - 1));
        const int x = glass.left() + int(t * glass.width()) + jitter(8);
        add(chem[i], QPoint(x, glass.top() + 52), i);
    }
    for (int i = 0; i < trap.size(); ++i) {
        const double t = (trap.size() == 1) ? 0.5 : (0.18 + 0.64 * i / (trap.size() - 1));
        const int x = glass.left() + int(t * glass.width()) + jitter(10);
        add(trap[i], QPoint(x, glass.bottom() - 48), i);
    }
    for (int i = 0; i < heat.size(); ++i) {
        const bool left = (i % 2) == 0;
        const int row = i / 2;
        const int x = left ? glass.left() + 18 : glass.right() - 18;
        const int y = glass.bottom() - 70 - row * 28 + jitter(4);
        add(heat[i], QPoint(x, y), i);
    }
    for (int i = 0; i < coolFans.size(); ++i) {
        const int wall = i % 3;
        const int row = i / 3;
        int x = glass.center().x();
        int y = glass.top() + 58 + row * 36;
        if (wall == 0)
            x = glass.left() + 24;
        else if (wall == 1)
            x = glass.right() - 24;
        else
            x = glass.center().x() + ((row % 2) ? -36 : 36);
        y += jitter(5);
        add(coolFans[i], QPoint(x, y), wall);
    }
}

void GreenhouseView::paintEvent(QPaintEvent *)
{
    QPainter p(this);
    const QRect r = rect();
    const auto cl = frame.value("climate").toObject();
    const auto opt = cl.value("optimal").toObject();
    const double t = cl.value("temperature").toDouble(22.0);
    const double rh = cl.value("humidity").toDouble(55.0);
    const double light = cl.value("light").toDouble(40.0);
    const double pests = cl.value("pests").toDouble(0.0);
    const double cover = cl.value("cloud_cover").toDouble();
    const QString action = cl.value("action").toString();
    const double shadeL = level.value(QStringLiteral("shade"));
    const double uvL = level.value(QStringLiteral("uv"));
    const double sprayL = level.value(QStringLiteral("spray_water"));
    const double chemL = level.value(QStringLiteral("chem"));
    const double heatL = level.value(QStringLiteral("heat"));
    const double coolL = level.value(QStringLiteral("cool"));
    const double ventL = level.value(QStringLiteral("vent"));

    QLinearGradient sky(0, 0, 0, r.height() * 0.45);
    const int skyG = qBound(70, int(130 + light * 0.85 - cover * 50), 210);
    const int skyB = qBound(90, int(210 - t * 1.6 - cover * 40), 235);
    sky.setColorAt(0, QColor(70, skyG, skyB));
    sky.setColorAt(1, QColor(160, 200, 170));
    p.fillRect(r, sky);

    const QPoint sun(r.width() - 56, 52);
    const int sunA = qBound(50, int(light * 2.4), 230);
    p.setPen(QPen(QColor(255, 200, 70, sunA), 2));
    for (int i = 0; i < 8; ++i) {
        const double a = i * 45.0 * M_PI / 180.0;
        p.drawLine(sun, sun + QPoint(int(28 * std::cos(a)), int(28 * std::sin(a))));
    }
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(255, 220, 80, sunA));
    p.drawEllipse(sun, 16, 16);
    p.setBrush(QColor(255, 250, 210, sunA));
    p.drawEllipse(sun, 9, 9);

    const auto clouds = cl.value("clouds").toArray();
    for (const auto &cv : clouds) {
        const auto o = cv.toObject();
        const double nx = o.value("x").toDouble();
        const double ny = o.value("y").toDouble();
        const double sz = o.value("size").toDouble(0.5);
        const QPointF c(r.width() * nx, r.height() * (0.02 + ny * 0.20));
        const double s = 28 + 64 * sz;
        const int alpha = qBound(120, int(170 + 50 * sz), 230);
        drawSimpleCloud(p, c, s, QColor(236, 242, 248, alpha));
    }

    const int groundTop = int(r.height() * 0.76);
    QLinearGradient earth(0, groundTop - 16, 0, r.height());
    earth.setColorAt(0.0, QColor(126, 168, 78));
    earth.setColorAt(0.12, QColor(96, 138, 58));
    earth.setColorAt(0.28, QColor(118, 86, 48));
    earth.setColorAt(1.0, QColor(72, 48, 28));
    p.fillRect(QRect(0, groundTop - 12, r.width(), r.height() - groundTop + 12), earth);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(92, 142, 52));
    for (int gx = -4; gx < r.width() + 8; gx += 22) {
        const int bump = (gx * 17 + 9) % 8;
        QPolygon tuft;
        const int gy = groundTop + 2 + bump / 3;
        tuft << QPoint(gx, gy + 4)
             << QPoint(gx + 3, gy - 8 - bump / 2)
             << QPoint(gx + 7, gy + 4);
        p.drawPolygon(tuft);
    }

    p.setRenderHint(QPainter::Antialiasing, true);

    QRect house(int(r.width() * 0.10), int(r.height() * 0.22),
                int(r.width() * 0.80), groundTop - int(r.height() * 0.22) + 8);
    QPolygon roof;
    roof << QPoint(house.left() - 12, house.top() + 40)
         << QPoint(house.center().x(), house.top() - 26)
         << QPoint(house.right() + 12, house.top() + 40);
    p.setBrush(QColor(176, 72, 58));
    p.setPen(QPen(QColor(90, 30, 25), 2));
    p.drawPolygon(roof);
    p.setPen(QPen(QColor(120, 50, 40), 1));
    p.drawLine(QPoint(house.center().x(), house.top() - 24),
               QPoint(house.center().x(), house.top() + 40));

    const int heatTint = qBound(0, int((t - 18.0) * 8), 90);
    p.setBrush(QColor(210, 240 - heatTint / 2, 215 - heatTint / 3, 175));
    p.setPen(QPen(QColor(70, 110, 90), 3));
    const QRect glass = house.adjusted(0, 36, 0, 0);
    p.drawRoundedRect(glass, 6, 6);
    p.setPen(QPen(QColor(90, 140, 110, 160), 2));
    p.drawLine(glass.center().x(), glass.top(), glass.center().x(), glass.bottom());
    p.drawLine(glass.left(), glass.top() + glass.height() / 2, glass.right(),
               glass.top() + glass.height() / 2);
    rebuildPlaced(house, glass);

    const QRect soil(glass.left() + 10, glass.bottom() - 40, glass.width() - 20, 32);
    p.setPen(Qt::NoPen);
    p.setBrush(QColor(92, 58, 32));
    p.drawRoundedRect(soil, 3, 3);

    const int plantH = 36 + int(rh * 0.55);
    auto drawPlant = [&](int x) {
        p.setPen(QPen(QColor(40, 100, 40), 3));
        p.drawLine(x, soil.top(), x, soil.top() - plantH);
        p.setBrush(QColor(48, 150, 68));
        p.setPen(Qt::NoPen);
        p.drawEllipse(QPoint(x - 13, soil.top() - plantH + 8), 15, 10);
        p.drawEllipse(QPoint(x + 13, soil.top() - plantH + 12), 15, 10);
        p.drawEllipse(QPoint(x, soil.top() - plantH - 5), 11, 11);
    };
    drawPlant(glass.left() + glass.width() / 4);
    drawPlant(glass.center().x());
    drawPlant(glass.right() - glass.width() / 4);

    bool hasUv = false;
    for (const auto &act : placed) {
        if (act.action == QLatin1String("uv")) {
            hasUv = true;
            break;
        }
    }
    if (hasUv) {
        p.setPen(QPen(QColor(90, 90, 110), 2));
        p.drawLine(glass.left() + 12, glass.top() + 8, glass.right() - 12, glass.top() + 8);
    }

    QSet<QString> captioned;
    auto captionOnce = [&](const QPoint &at, const QString &name, int alpha) {
        if (captioned.contains(name))
            return;
        captioned.insert(name);
        drawCaption(p, QRect(at.x() - 40, at.y() - 18, 80, 14), name, alpha);
    };
    for (const auto &act : placed) {
        const int x = act.pos.x();
        const int y = act.pos.y();
        if (act.action == QLatin1String("shade")) {
            const int hw = int(8 + 34 * shadeL);
            const int hh = int(12 + 26 * shadeL);
            p.setPen(QPen(QColor(50, 40, 30), 2));
            p.drawLine(QPoint(x, y + 2), QPoint(x, y + 18));
            p.drawArc(QRect(x - 5, y + 14, 10, 10), 180 * 16, 160 * 16);
            p.setBrush(QColor(70, 80, 120, int(80 + 140 * shadeL)));
            p.setPen(QPen(QColor(40, 45, 70), 1));
            p.drawChord(QRect(x - hw, y - hh, 2 * hw, hh + 12), 0, 180 * 16);
            p.setPen(QPen(QColor(40, 45, 70, 160), 1));
            for (int rib = -2; rib <= 2; ++rib)
                p.drawLine(QPoint(x, y - hh + 4), QPoint(x + rib * hw / 2, y + 2));
            captionOnce(QPoint(x, y - hh), act.name, int(90 + 140 * shadeL));
        } else if (act.action == QLatin1String("uv")) {
            p.setPen(QPen(QColor(90, 90, 110), 1));
            p.drawLine(x, glass.top(), x, y + 4);
            p.setBrush(QColor(90, 90, 110));
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(QRect(x - 18, y, 36, 9), 4, 4);
            p.setBrush(QColor(210, 160, 255, int(40 + 180 * uvL)));
            p.drawRoundedRect(QRect(x - 14, y + 2, 28, 5), 3, 3);
            if (uvL > 0.05) {
                p.setBrush(QColor(180, 120, 255, int(40 * uvL)));
                p.drawRect(x - 12, y + 9, 24, int(22 * uvL));
            }
            captionOnce(QPoint(x, y + 16), act.name, int(90 + 140 * uvL));
        } else if (act.action == QLatin1String("spray_water")) {
            p.setBrush(QColor(110, 120, 140));
            p.setPen(QPen(QColor(70, 80, 100), 1));
            p.drawEllipse(QPoint(x, y), 9, 5);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(50, 60, 80));
            for (int h = 0; h < 5; ++h)
                p.drawEllipse(QPoint(x - 4 + h * 2, y + 1), 1, 1);
            if (sprayL > 0.03) {
                p.setBrush(QColor(80, 160, 230, int(50 + 130 * sprayL)));
                for (int d = 0; d < 7; ++d) {
                    const int drop = int((phase * 6 + act.variant * 17 + d * 11) % 70 * sprayL);
                    p.drawEllipse(QPoint(x + (d - 3) * 3, y + 8 + drop), 2, int(3 + 3 * sprayL));
                }
            }
            captionOnce(QPoint(x, y - 8), act.name, int(90 + 140 * sprayL));
        } else if (act.name.contains(QString::fromUtf8("ловуш"))) {
            p.setPen(QPen(QColor(90, 70, 40), 1));
            p.setBrush(QColor(210, 190, 90, 210));
            p.drawRoundedRect(QRect(x - 10, y - 14, 20, 28), 3, 3);
            p.setPen(QPen(QColor(40, 30, 20), 1));
            p.drawLine(x - 6, y - 8, x + 6, y + 8);
            p.drawLine(x + 6, y - 8, x - 6, y + 8);
            captionOnce(QPoint(x, y - 20), act.name, int(90 + 140 * chemL));
        } else if (act.action == QLatin1String("chem")) {
            p.setBrush(QColor(60, 110, 50));
            p.setPen(QPen(QColor(40, 80, 35), 1));
            p.drawRoundedRect(QRect(x - 6, y, 12, 16), 2, 2);
            p.drawEllipse(QPoint(x, y), 5, 4);
            if (chemL > 0.04) {
                const double s = 18 + 22 * chemL;
                QPointF c(x + 4 * std::sin((phase + act.variant * 20) * 0.08),
                          y + 22 + int(18 * (1.0 - chemL))
                              + int(6 * std::sin((phase + act.variant * 12) * 0.1)));
                drawSimpleCloud(p, c, s, QColor(130, 210, 80, int(50 + 90 * chemL)));
            }
            captionOnce(QPoint(x, y - 12), act.name, int(90 + 140 * chemL));
        } else if (act.action == QLatin1String("heat")) {
            const QRect rad(x - 11, y - 22, 22, 44);
            p.setPen(QPen(QColor(90, 55, 40), 1));
            p.setBrush(QColor(150, 95, 70, 210));
            p.drawRoundedRect(rad, 2, 2);
            p.setPen(QPen(QColor(90, 55, 40), 2));
            for (int f = 0; f < 5; ++f)
                p.drawLine(rad.left() + 4 + f * 4, rad.top() + 4, rad.left() + 4 + f * 4, rad.bottom() - 4);
            if (heatL > 0.04) {
                p.setPen(QPen(QColor(230, 90, 40, int(50 + 150 * heatL)), 2));
                for (int i = 0; i < 4; ++i) {
                    const int wobble = int(4 * heatL * std::sin((phase + i * 20) * 0.14));
                    p.drawArc(QRect(rad.left() - 2 + wobble, rad.top() - 14 - i * 5, 26, 16), 0, 180 * 16);
                }
            }
            captionOnce(QPoint(x, rad.top() - 4), act.name, int(90 + 140 * heatL));
        } else if (act.action == QLatin1String("vent")) {
            drawRoofVent(p, act.pos, ventL, phase);
            captionOnce(QPoint(x, y - 40), act.name, int(90 + 140 * ventL));
        } else {
            drawWallFan(p, act.pos, coolL, phase, act.variant);
            captionOnce(QPoint(x, y + 22), act.name, int(90 + 140 * coolL));
        }
    }

    if (pests > 3.0) {
        const int nbugs = qBound(1, int(pests / 8), 10);
        for (int i = 0; i < nbugs; ++i) {
            const int x = glass.left() + 30 + (i * 61 + phase) % qMax(1, glass.width() - 60);
            const int y = soil.top() - 18 - (i * 13) % qMax(10, plantH);
            p.setPen(QPen(QColor(40, 22, 12), 1));
            p.setBrush(QColor(72, 42, 22));
            p.drawEllipse(QPoint(x, y), 5, 3);
            p.drawEllipse(QPoint(x + 5, y - 1), 3, 2);
            p.drawLine(QPoint(x + 7, y - 2), QPoint(x + 10, y - 5));
            p.drawLine(QPoint(x + 7, y - 1), QPoint(x + 10, y - 3));
            p.setPen(QPen(QColor(40, 22, 12), 1));
            p.drawLine(QPoint(x - 3, y), QPoint(x - 7, y + 3));
            p.drawLine(QPoint(x, y + 2), QPoint(x, y + 5));
            p.drawLine(QPoint(x + 3, y), QPoint(x + 6, y + 3));
        }
        drawCaption(p, QRect(glass.center().x() - 40, soil.top() - plantH - 18, 80, 14),
                    QString::fromUtf8("вредители"), 200);
    }

    QString actText = QString::fromUtf8("выкл");
    if (action == QLatin1String("heat"))
        actText = QString::fromUtf8("нагрев");
    else if (action == QLatin1String("cool"))
        actText = QString::fromUtf8("охлаждение");
    else if (action == QLatin1String("spray_water"))
        actText = QString::fromUtf8("полив");
    else if (action == QLatin1String("vent"))
        actText = QString::fromUtf8("вентиляция");
    else if (action == QLatin1String("uv"))
        actText = QString::fromUtf8("UV-лампы");
    else if (action == QLatin1String("shade"))
        actText = QString::fromUtf8("зонты");
    else if (action == QLatin1String("chem"))
        actText = QString::fromUtf8("химикаты");

    p.setPen(QColor(20, 30, 20));
    p.setFont(QFont("Sans", 10, QFont::DemiBold));
    const QString head = QString::fromUtf8(
                             "оптимум: температура %1 C   влажность %2 %%   свет %3   вредители %4\n"
                             "сейчас: температура %5 C   влажность %6 %%   свет %7   вредители %8   CO2 %9\n")
                             .arg(opt.value("temperature").toDouble(24.0), 0, 'f', 1)
                             .arg(opt.value("humidity").toDouble(70.0), 0, 'f', 1)
                             .arg(opt.value("light").toDouble(55.0), 0, 'f', 1)
                             .arg(opt.value("pests").toDouble(0.0), 0, 'f', 0)
                             .arg(t, 0, 'f', 1)
                             .arg(rh, 0, 'f', 1)
                             .arg(light, 0, 'f', 1)
                             .arg(pests, 0, 'f', 1)
                             .arg(cl.value("co2").toDouble(), 0, 'f', 0);
    p.drawText(QRect(8, 4, r.width() - 90, 78),
               Qt::AlignTop | Qt::AlignLeft,
               head + QString::fromUtf8("команда: %1   облака: %2")
                          .arg(actText)
                          .arg(cover, 0, 'f', 2));
}
