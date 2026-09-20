#include <QCommandLineParser>
#include <QtWidgets>
#include <QProcess>
#include <QJsonDocument>
#include <QJsonObject>
#include <QJsonArray>
#include <QTimer>
#include <QRandomGenerator>
#include <cmath>

#ifndef M_PI
#define M_PI 3.14159265358979323846
#endif

struct Fly {
    QPointF a, b;
    double u = 0.0;
    QColor color = QColor(40, 90, 200);
};

static void drawFluffyCloud(QPainter &p, QPointF c, double s, const QColor &fill)
{
    p.setPen(QPen(fill.darker(115), 1));
    p.setBrush(fill);
    p.drawEllipse(c + QPointF(-0.42 * s, 0.04 * s), 0.40 * s, 0.26 * s);
    p.drawEllipse(c + QPointF(0.02 * s, -0.14 * s), 0.48 * s, 0.32 * s);
    p.drawEllipse(c + QPointF(0.40 * s, 0.02 * s), 0.36 * s, 0.24 * s);
    p.drawEllipse(c + QPointF(-0.08 * s, 0.14 * s), 0.38 * s, 0.20 * s);
}

static void drawCaption(QPainter &p, const QRect &at, const QString &text, int alpha)
{
    p.setPen(QColor(25, 35, 25, qBound(70, alpha, 230)));
    p.setFont(QFont("Sans", 7, QFont::DemiBold));
    p.drawText(at, Qt::AlignCenter, text);
}

static QString groupTitle(int src)
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

enum class PipeKind { Inbound, Outbound, Combined };

class GreenhouseView : public QWidget
{
public:
    QJsonObject frame;
    int phase = 0;
    QMap<QString, double> level;

    explicit GreenhouseView(QWidget *parent = nullptr) : QWidget(parent)
    {
        setMinimumSize(480, 360);
        const QStringList keys = {
            QStringLiteral("shade"), QStringLiteral("uv"), QStringLiteral("spray_water"),
            QStringLiteral("chem"), QStringLiteral("heat"), QStringLiteral("cool"),
            QStringLiteral("vent")
        };
        for (const auto &k : keys)
            level[k] = 0.0;
        auto *tick = new QTimer(this);
        connect(tick, &QTimer::timeout, this, [this, keys]() {
            const QString action = frame.value("climate").toObject().value("action").toString();
            for (const auto &k : keys) {
                const double target = (action == k) ? 1.0 : 0.0;
                double &v = level[k];
                if (v < target)
                    v = qMin(target, v + 0.20);
                else
                    v = qMax(target, v - 0.12);
            }
            phase = (phase + 1) % 360;
            update();
        });
        tick->start(40);
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
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
        const double ventL = qMax(level.value(QStringLiteral("cool")),
                                  level.value(QStringLiteral("vent")));

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
            const double a = (i * 45 + phase * 0.4) * M_PI / 180.0;
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
            QPointF c(r.width() * nx, r.height() * (0.02 + ny * 0.20));
            c.ry() += 3.0 * std::sin((phase + sz * 50) * 0.06);
            const double s = 28 + 64 * sz;
            const int alpha = qBound(120, int(170 + 50 * sz), 230);
            drawFluffyCloud(p, c, s, QColor(236, 242, 248, alpha));
        }

        QRect house(int(r.width() * 0.10), int(r.height() * 0.30),
                    int(r.width() * 0.80), int(r.height() * 0.62));
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

        for (int i = 0; i < 3; ++i) {
            const int x = house.left() + house.width() * (i + 1) / 4;
            const int y = house.top() + 6;
            const int hw = int(8 + 34 * shadeL);
            const int hh = int(12 + 26 * shadeL);
            p.setPen(QPen(QColor(50, 40, 30), 2));
            p.drawLine(QPoint(x, y + 2), QPoint(x, y + 22));
            p.drawArc(QRect(x - 5, y + 18, 10, 10), 180 * 16, 160 * 16);
            p.setBrush(QColor(70, 80, 120, int(80 + 140 * shadeL)));
            p.setPen(QPen(QColor(40, 45, 70), 1));
            p.drawChord(QRect(x - hw, y - hh, 2 * hw, hh + 12), 0, 180 * 16);
            p.setPen(QPen(QColor(40, 45, 70, 160), 1));
            for (int rib = -2; rib <= 2; ++rib)
                p.drawLine(QPoint(x, y - hh + 4), QPoint(x + rib * hw / 2, y + 2));
        }
        drawCaption(p, QRect(house.center().x() - 40, house.top() - 38, 80, 14),
                    QString::fromUtf8("зонты"), int(90 + 140 * shadeL));

        for (int i = 0; i < 3; ++i) {
            const int x = glass.left() + 28 + i * (glass.width() - 56) / 2;
            const int y = glass.top() + 8;
            p.setPen(QPen(QColor(90, 90, 110), 1));
            p.drawLine(x, glass.top(), x, y + 4);
            p.setBrush(QColor(90, 90, 110));
            p.setPen(Qt::NoPen);
            p.drawRoundedRect(QRect(x - 22, y, 44, 10), 4, 4);
            p.setBrush(QColor(210, 160, 255, int(40 + 180 * uvL)));
            p.drawRoundedRect(QRect(x - 18, y + 2, 36, 6), 3, 3);
            if (uvL > 0.05) {
                p.setBrush(QColor(180, 120, 255, int(40 * uvL)));
                p.drawRect(x - 16, y + 10, 32, int(28 * uvL));
            }
        }
        drawCaption(p, QRect(glass.left() + 8, glass.top() + 20, 70, 14),
                    QString::fromUtf8("UV-лампы"), int(90 + 140 * uvL));

        p.setPen(QPen(QColor(90, 100, 120), 2));
        p.drawLine(glass.left() + 16, glass.top() + 28, glass.right() - 16, glass.top() + 28);
        for (int i = 0; i < 4; ++i) {
            const int x = glass.left() + 40 + i * (glass.width() - 80) / 3;
            p.setBrush(QColor(110, 120, 140));
            p.setPen(QPen(QColor(70, 80, 100), 1));
            p.drawEllipse(QPoint(x, glass.top() + 36), 9, 5);
            p.setPen(Qt::NoPen);
            p.setBrush(QColor(50, 60, 80));
            for (int h = 0; h < 5; ++h)
                p.drawEllipse(QPoint(x - 4 + h * 2, glass.top() + 37), 1, 1);
            if (sprayL > 0.03) {
                p.setBrush(QColor(80, 160, 230, int(50 + 130 * sprayL)));
                for (int d = 0; d < 7; ++d) {
                    const int drop = int((phase * 6 + i * 17 + d * 11) % 70 * sprayL);
                    const int dx = (d - 3) * 3;
                    p.drawEllipse(QPoint(x + dx, glass.top() + 42 + drop), 2, int(3 + 3 * sprayL));
                }
            }
        }
        drawCaption(p, QRect(glass.right() - 90, glass.top() + 8, 84, 14),
                    QString::fromUtf8("душ"), int(90 + 140 * sprayL));

        p.setPen(QPen(QColor(80, 90, 110), 3));
        p.drawLine(soil.left() + 6, soil.top() - 6, soil.right() - 6, soil.top() - 6);
        p.setPen(Qt::NoPen);
        for (int i = 0; i < 8; ++i) {
            const int x = soil.left() + 14 + i * (soil.width() - 28) / 7;
            p.setBrush(QColor(80, 90, 110));
            p.drawRect(x - 2, soil.top() - 10, 4, 6);
            if (sprayL > 0.03) {
                const int drop = int((phase * 4 + i * 9) % 14 * sprayL);
                p.setBrush(QColor(70, 150, 220, int(60 + 140 * sprayL)));
                p.drawEllipse(QPoint(x, soil.top() - 2 + drop), 2, 4);
            }
        }
        drawCaption(p, QRect(soil.left(), soil.top() - 24, 110, 14),
                    QString::fromUtf8("капельный полив"), int(90 + 140 * sprayL));

        for (int i = 0; i < 3; ++i) {
            const int x = glass.left() + 50 + i * (glass.width() - 100) / 2;
            p.setBrush(QColor(60, 110, 50));
            p.setPen(QPen(QColor(40, 80, 35), 1));
            p.drawRoundedRect(QRect(x - 6, glass.top() + 48, 12, 16), 2, 2);
            p.drawEllipse(QPoint(x, glass.top() + 48), 5, 4);
            if (chemL > 0.04) {
                const double s = 18 + 22 * chemL;
                QPointF c(x + 4 * std::sin((phase + i * 20) * 0.08),
                          glass.top() + 70 + int(18 * (1.0 - chemL))
                              + int(6 * std::sin((phase + i * 12) * 0.1)));
                drawFluffyCloud(p, c, s, QColor(130, 210, 80, int(50 + 90 * chemL)));
            }
        }
        drawCaption(p, QRect(glass.center().x() - 40, glass.top() + 88, 80, 14),
                    QString::fromUtf8("химикаты"), int(90 + 140 * chemL));

        const QRect rad(glass.left() + 8, glass.bottom() - 92, 22, 44);
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
        drawCaption(p, QRect(rad.left() - 8, rad.top() - 16, 50, 14),
                    QString::fromUtf8("нагрев"), int(90 + 140 * heatL));

        const QPoint fanC(glass.right() - 28, glass.top() + 70);
        p.setPen(QPen(QColor(80, 90, 110), 2));
        p.setBrush(QColor(190, 200, 210, 200));
        p.drawRoundedRect(QRect(fanC.x() - 18, fanC.y() - 18, 36, 36), 4, 4);
        p.setBrush(QColor(170, 185, 200));
        p.drawEllipse(fanC, 14, 14);
        p.setBrush(QColor(90, 110, 140, int(120 + 80 * ventL)));
        for (int b = 0; b < 3; ++b) {
            const double a = (phase * (4.0 + 14.0 * ventL) + b * 120.0) * M_PI / 180.0;
            QPolygonF blade;
            blade << fanC
                  << QPointF(fanC.x() + 12 * std::cos(a - 0.35), fanC.y() + 12 * std::sin(a - 0.35))
                  << QPointF(fanC.x() + 13 * std::cos(a), fanC.y() + 13 * std::sin(a))
                  << QPointF(fanC.x() + 12 * std::cos(a + 0.35), fanC.y() + 12 * std::sin(a + 0.35));
            p.drawPolygon(blade);
        }
        p.setBrush(QColor(60, 70, 90));
        p.setPen(Qt::NoPen);
        p.drawEllipse(fanC, 3, 3);
        if (ventL > 0.05) {
            p.setPen(QPen(QColor(90, 150, 220, int(50 + 140 * ventL)), 2));
            for (int i = 0; i < 3; ++i)
                p.drawLine(fanC.x() - 22 - int(10 * ventL), fanC.y() - 8 + i * 8,
                           fanC.x() - 34 - int(16 * ventL), fanC.y() - 4 + i * 8);
        }
        drawCaption(p, QRect(fanC.x() - 40, fanC.y() + 20, 80, 14),
                    QString::fromUtf8("вентилятор"), int(90 + 140 * ventL));

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
};

class PipelineView : public QWidget
{
public:
    PipeKind kind = PipeKind::Combined;
    QJsonObject frame;
    QVector<Fly> flies;
    QString hot;

    explicit PipelineView(PipeKind k, QWidget *parent = nullptr)
        : QWidget(parent), kind(k)
    {
        setMinimumSize(280, k == PipeKind::Combined ? 430 : (k == PipeKind::Outbound ? 210 : 180));
        auto *tick = new QTimer(this);
        connect(tick, &QTimer::timeout, this, [this]() {
            for (int i = flies.size() - 1; i >= 0; --i) {
                flies[i].u += 0.045;
                if (flies[i].u >= 1.0)
                    flies.removeAt(i);
            }
            update();
        });
        tick->start(33);
    }

    void pulse(const QJsonObject &o)
    {
        frame = o;
        const QString ev = o.value("event").toString();
        const bool inboundEv = ev.contains(QString::fromUtf8("сгенерирована"))
                               || ev.contains(QString::fromUtf8("выбивание"));
        const bool outboundEv = ev.contains(QString::fromUtf8("обработка"))
                                || ev.contains(QString::fromUtf8("отправка"))
                                || ev.contains(QString::fromUtf8("прибор"))
                                || ev.contains(QString::fromUtf8("выдача"));
        if (kind == PipeKind::Inbound && !inboundEv) {
            update();
            return;
        }
        if (kind == PipeKind::Outbound && !outboundEv) {
            update();
            return;
        }
        if (inboundEv)
            hot = QStringLiteral("src");
        else if (outboundEv)
            hot = QStringLiteral("dv");
        else
            hot = QStringLiteral("buf");
        spawnFlies(o.value("request").toString());
        update();
    }

protected:
    void paintEvent(QPaintEvent *) override
    {
        QPainter p(this);
        p.setRenderHint(QPainter::Antialiasing);
        p.fillRect(rect(), QColor(248, 248, 246));
        p.setPen(QColor(40, 40, 40));
        p.setFont(QFont("Sans", 10, QFont::DemiBold));
        const QString title = kind == PipeKind::Inbound
                                  ? QString::fromUtf8("Приём в буфер")
                                  : kind == PipeKind::Outbound
                                        ? QString::fromUtf8("Выдача из буфера")
                                        : QString::fromUtf8("Обработка сигнала");
        p.drawText(QRect(8, 4, width() - 16, 18), Qt::AlignLeft, title);

        const auto n = nodeRects();
        auto drawEdge = [&](const QString &a, const QString &b) {
            if (!n.contains(a) || !n.contains(b))
                return;
            p.setPen(QPen(QColor(160, 160, 160), 2));
            p.drawLine(n[a].center(), n[b].center());
        };
        if (kind != PipeKind::Outbound) {
            drawEdge("s1", "dp");
            drawEdge("s2", "dp");
            drawEdge("s3", "dp");
            drawEdge("s4", "dp");
            drawEdge("dp", "buf");
        }
        if (kind != PipeKind::Inbound) {
            drawEdge("buf", "dv");
            for (const auto &id : deviceNodeIds())
                drawEdge("dv", id);
        }

        auto drawNode = [&](const QString &id, const QString &label, const QColor &base) {
            if (!n.contains(id))
                return;
            const bool on = (hot == id) || (hot == QLatin1String("src") && id.startsWith('s'))
                            || (hot == QLatin1String("dv") && (id == QLatin1String("dv")
                                                              || id.startsWith(QLatin1Char('p'))))
                            || (hot == QLatin1String("w") && id.startsWith(QLatin1Char('p')));
            p.setBrush(on ? base.lighter(130) : QColor(255, 255, 255));
            p.setPen(QPen(on ? base.darker(140) : QColor(120, 120, 120), on ? 3 : 1));
            p.drawRoundedRect(n[id], 8, 8);
            p.setPen(QColor(30, 30, 30));
            p.setFont(QFont("Sans", 8));
            p.drawText(n[id], Qt::AlignCenter, label);
        };
        drawNode("s1", QString::fromUtf8("И1 темп"), QColor(200, 80, 70));
        drawNode("s2", QString::fromUtf8("И2 влажн"), QColor(70, 120, 190));
        drawNode("s3", QString::fromUtf8("И3 свет"), QColor(210, 170, 40));
        drawNode("s4", QString::fromUtf8("И4 вредит"), QColor(90, 120, 50));
        drawNode("dp", QString::fromUtf8("ДП"), QColor(90, 90, 140));
        drawNode("buf", QString::fromUtf8("БП"), QColor(80, 130, 90));
        drawNode("dv", QString::fromUtf8("ДВ"), QColor(90, 90, 140));
        p.setFont(QFont("Sans", 7));
        for (const auto &g : metricGroups()) {
            if (g.ids.isEmpty())
                continue;
            const QString first = QStringLiteral("p%1").arg(g.ids.first());
            const QString last = QStringLiteral("p%1").arg(g.ids.last());
            if (!n.contains(first) || !n.contains(last))
                continue;
            const QRect col = n[first].united(n[last]).adjusted(-5, -16, 5, 5);
            const QColor colr = sourceColor(g.source);
            p.setBrush(QColor(colr.red(), colr.green(), colr.blue(), 28));
            p.setPen(QPen(colr.darker(120), 1, Qt::DashLine));
            p.drawRoundedRect(col, 6, 6);
            p.setPen(colr.darker(140));
            p.drawText(QRect(col.left(), col.top() + 1, col.width(), 12), Qt::AlignCenter, groupTitle(g.source));
        }
        for (const auto &g : metricGroups()) {
            const QColor col = sourceColor(g.source);
            for (int id : g.ids)
                drawNode(QStringLiteral("p%1").arg(id), QStringLiteral("P%1").arg(id), col);
        }

        const auto buf = frame.value("buffer").toArray();
        p.setFont(QFont("Sans", 8));
        p.setPen(QColor(60, 60, 60));
        QStringList ids;
        for (const auto &v : buf)
            ids << v.toString();
        if (n.contains("buf"))
            p.drawText(QRect(8, n["buf"].bottom() + 2, width() - 16, 16), Qt::AlignCenter,
                       ids.isEmpty() ? QString("-") : ids.join(QStringLiteral(" · ")));

        for (const auto &f : flies) {
            const QPointF pos = f.a + (f.b - f.a) * f.u;
            p.setPen(Qt::NoPen);
            p.setBrush(f.color);
            p.drawEllipse(pos, 6, 6);
        }
    }

private:
    struct MetricGroup {
        int source = 0;
        QList<int> ids;
    };

    static QColor sourceColor(int src)
    {
        if (src == 2)
            return QColor(70, 120, 190);
        if (src == 3)
            return QColor(210, 170, 40);
        if (src == 4)
            return QColor(90, 120, 50);
        return QColor(200, 80, 70);
    }

    QList<MetricGroup> metricGroups() const
    {
        QList<MetricGroup> out;
        const auto arr = frame.value("groups").toArray();
        if (!arr.isEmpty()) {
            for (const auto &v : arr) {
                MetricGroup g;
                g.source = v.toObject().value("source").toInt();
                for (const auto &id : v.toObject().value("devices").toArray())
                    g.ids.append(id.toInt());
                if (!g.ids.isEmpty())
                    out.append(g);
            }
            return out;
        }
        for (int s = 1; s <= 4; ++s)
            out.append({s, {2 * s - 1, 2 * s}});
        return out;
    }

    QStringList deviceNodeIds() const
    {
        QStringList ids;
        for (const auto &g : metricGroups()) {
            for (int id : g.ids)
                ids << QStringLiteral("p%1").arg(id);
        }
        return ids;
    }

    QHash<QString, QRect> nodeRects() const
    {
        QHash<QString, QRect> m;
        const int w = 54, h = 26;
        const int cx = width() / 2;
        const int top = 24;
        const int span = qMax(1, width() - 16);
        const int gap = qMax(4, (span - 4 * w) / 3);
        auto srcRect = [&](int i) { return QRect(8 + i * (w + gap), top, w, h); };
        auto placeDevices = [&](int y0) {
            const auto groups = metricGroups();
            const int cols = qMax(1, groups.size());
            const int boxW = 40;
            const int boxH = 24;
            const int vGap = 6;
            const int avail = qMax(boxW, width() - 16);
            const int colGap = cols > 1 ? qMax(14, (avail - cols * boxW) / (cols - 1)) : 0;
            for (int c = 0; c < cols; ++c) {
                const auto &g = groups[c];
                const int x = 8 + c * (boxW + colGap);
                for (int i = 0; i < g.ids.size(); ++i) {
                    const int y = y0 + i * (boxH + vGap);
                    m[QStringLiteral("p%1").arg(g.ids[i])] = QRect(x, y, boxW, boxH);
                }
            }
        };
        if (kind == PipeKind::Inbound) {
            m["s1"] = srcRect(0);
            m["s2"] = srcRect(1);
            m["s3"] = srcRect(2);
            m["s4"] = srcRect(3);
            m["dp"] = QRect(cx - w / 2, top + 50, w, h);
            m["buf"] = QRect(cx - 48, top + 100, 96, 34);
        } else if (kind == PipeKind::Outbound) {
            m["buf"] = QRect(cx - 48, top, 96, 34);
            m["dv"] = QRect(cx - w / 2, top + 46, w, h);
            placeDevices(top + 92);
        } else {
            m["s1"] = srcRect(0);
            m["s2"] = srcRect(1);
            m["s3"] = srcRect(2);
            m["s4"] = srcRect(3);
            m["dp"] = QRect(cx - w / 2, 88, w, h);
            m["buf"] = QRect(cx - 50, 140, 100, 34);
            m["dv"] = QRect(cx - w / 2, 190, w, h);
            placeDevices(236);
        }
        return m;
    }

    void spawnFlies(const QString &req)
    {
        const auto n = nodeRects();
        auto add = [&](const QString &a, const QString &b, const QColor &c) {
            if (!n.contains(a) || !n.contains(b))
                return;
            Fly f;
            f.a = n[a].center();
            f.b = n[b].center();
            f.color = c;
            flies.append(f);
        };
        const QChar srcCh = req.size() && req[0].isDigit() ? req[0] : QChar('1');
        const int src = srcCh.digitValue();
        QString sid = QStringLiteral("s1");
        if (src == 2)
            sid = QStringLiteral("s2");
        else if (src == 3)
            sid = QStringLiteral("s3");
        else if (src == 4)
            sid = QStringLiteral("s4");
        const QColor col = sourceColor(src);
        if (kind == PipeKind::Inbound || (kind == PipeKind::Combined && hot == QLatin1String("src"))) {
            add(sid, QStringLiteral("dp"), col);
            add(QStringLiteral("dp"), QStringLiteral("buf"), col);
        }
        if (kind == PipeKind::Outbound || (kind == PipeKind::Combined && hot == QLatin1String("dv"))) {
            add(QStringLiteral("buf"), QStringLiteral("dv"), col);
            QList<int> ids;
            for (const auto &g : metricGroups()) {
                if (g.source == src)
                    ids = g.ids;
            }
            if (ids.isEmpty())
                ids = {1, 2};
            for (int id : ids)
                add(QStringLiteral("dv"), QStringLiteral("p%1").arg(id), col);
        }
        while (flies.size() > 22)
            flies.removeFirst();
    }
};

struct SimLaunch {
    int sources = 4;
    int devices = 0;
    int buffer = 4;
    int seed = 42;
    int nRequests = 0;
    double mu = 0.55;
    double a = 1.0;
    double b = 3.0;
    double bufferDwell = 0.55;
    double temp = 22.0;
    double humidity = 55.0;
    double light = 40.0;
    double soil = 45.0;
    double co2 = 400.0;
    double pests = 18.0;
    double optTemp = 24.0;
    double optHumidity = 70.0;
    double optLight = 55.0;
    double optPests = 0.0;
    QString smoPath;
};

static SimLaunch parseLaunch(QCoreApplication &app)
{
    QCommandLineParser p;
    p.setApplicationDescription(QString::fromUtf8(
        "Qt-симулятор теплицы. Запускает smo.py step --infinite --json-lines --pull\n"
        "с теми же флагами модели, что и ./smo.py step (оптимум, буфер, источники, климат)."));
    p.addHelpOption();

    auto add = [&](const QStringList &names, const char *help, const char *valueName, const char *def) {
        QCommandLineOption opt(names, QString::fromUtf8(help), QString::fromUtf8(valueName), QString::fromUtf8(def));
        p.addOption(opt);
        return opt;
    };

    const auto sources = add({"sources"}, "число источников И1..Иn", "N", "4");
    const auto devices = add(
        {"devices"}, "всего приборов; 0 = по 2 прибора на каждый источник", "N", "0");
    const auto buffer = add({"buffer"}, "ёмкость общей БП", "N", "4");
    const auto seed = add({"seed"}, "зерно ГПСЧ", "N", "42");
    const auto mu = add({"mu"}, "интенсивность обслуживания Exp(mu), ПЗ1", "X", "0.55");
    const auto a = add({"a"}, "нижняя граница U[a,b], ИЗ2", "X", "1.0");
    const auto b = add({"b"}, "верхняя граница U[a,b], ИЗ2", "X", "3.0");
    const auto dwell = add(
        {"buffer-dwell"},
        "задержка выдачи из БП (очередь видна, редкие отказы)",
        "T",
        "0.55");
    const auto n = add({"n"}, "сколько заявок; 0 = бесконечно (--infinite)", "N", "0");
    const auto temp = add({"temp"}, "стартовая температура, C", "X", "22.0");
    const auto humidity = add({"humidity"}, "стартовая влажность, percent", "X", "55.0");
    const auto light = add({"light"}, "стартовая освещённость, 0..100", "X", "40.0");
    const auto soil = add({"soil"}, "стартовая влажность почвы", "X", "45.0");
    const auto co2 = add({"co2"}, "стартовый CO2, ppm", "X", "400.0");
    const auto pests = add({"pests"}, "стартовый уровень вредителей, 0..100", "X", "18.0");
    const auto optTemp = add({"opt-temp"}, "оптимум температуры роста, C", "X", "24.0");
    const auto optHumidity = add({"opt-humidity"}, "оптимум влажности роста, percent", "X", "70.0");
    const auto optLight = add({"opt-light"}, "оптимум света роста, 0..100", "X", "55.0");
    const auto optPests = add({"opt-pests"}, "оптимум вредителей (обычно 0)", "X", "0.0");
    const auto smo = add({"smo"}, "путь к скрипту smo.py (по умолчанию ../smo.py от бинарника)", "PATH", "");

    p.process(app);

    SimLaunch o;
    o.sources = p.value(sources).toInt();
    o.devices = p.value(devices).toInt();
    o.buffer = p.value(buffer).toInt();
    o.seed = p.value(seed).toInt();
    o.nRequests = p.value(n).toInt();
    o.mu = p.value(mu).toDouble();
    o.a = p.value(a).toDouble();
    o.b = p.value(b).toDouble();
    o.bufferDwell = p.value(dwell).toDouble();
    o.temp = p.value(temp).toDouble();
    o.humidity = p.value(humidity).toDouble();
    o.light = p.value(light).toDouble();
    o.soil = p.value(soil).toDouble();
    o.co2 = p.value(co2).toDouble();
    o.pests = p.value(pests).toDouble();
    o.optTemp = p.value(optTemp).toDouble();
    o.optHumidity = p.value(optHumidity).toDouble();
    o.optLight = p.value(optLight).toDouble();
    o.optPests = p.value(optPests).toDouble();
    o.smoPath = p.value(smo);
    return o;
}

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setApplicationName(QStringLiteral("greenhouse-sim"));
    const SimLaunch launch = parseLaunch(app);

    QWidget window;
    window.setWindowTitle(QString::fromUtf8("Теплица - вычислительный контур СМО, вариант 22"));
    window.setMinimumSize(1100, 700);

    auto *view = new GreenhouseView;
    auto *pipeIn = new PipelineView(PipeKind::Inbound);
    auto *pipeOut = new PipelineView(PipeKind::Outbound);
    auto *pipeAll = new PipelineView(PipeKind::Combined);
    pipeAll->hide();
    auto *event = new QLabel(QString::fromUtf8("Запуск модели..."));
    event->setWordWrap(true);
    auto *buffer = new QLabel("-");
    auto *devices = new QLabel("-");
    devices->setWordWrap(true);
    auto *speedLbl = new QLabel(QString::fromUtf8("скорость x1.0"));
    auto *log = new QPlainTextEdit;
    log->setReadOnly(true);
    log->setFocusPolicy(Qt::ClickFocus);
    log->setMaximumBlockCount(300);

    auto *pauseBtn = new QPushButton(QString::fromUtf8("Пауза (пробел)"));
    auto *slowerBtn = new QPushButton(QString::fromUtf8("Медленнее (-)"));
    auto *fasterBtn = new QPushButton(QString::fromUtf8("Быстрее (+)"));
    auto *schemeBtn = new QPushButton(QString::fromUtf8("Схема: один блок"));
    auto *weatherBtn = new QPushButton(QString::fromUtf8("Случайная погода"));
    auto *startBtn = new QPushButton(QString::fromUtf8("Перезапустить"));

    auto *toolbar = new QWidget;
    auto *btns = new QHBoxLayout(toolbar);
    btns->setContentsMargins(8, 6, 8, 6);
    btns->addWidget(pauseBtn);
    btns->addWidget(slowerBtn);
    btns->addWidget(fasterBtn);
    btns->addWidget(speedLbl);
    btns->addWidget(schemeBtn);
    btns->addStretch();
    btns->addWidget(weatherBtn);
    btns->addWidget(startBtn);

    auto *mid = new QVBoxLayout;
    mid->addWidget(new QLabel(QString::fromUtf8("Событие / выбор Д2Б4 / указатель Д2П2")));
    mid->addWidget(event);
    mid->addWidget(new QLabel(QString::fromUtf8("Буфер БП")));
    mid->addWidget(buffer);
    mid->addWidget(new QLabel(QString::fromUtf8("Приборы-воркеры")));
    mid->addWidget(devices);
    mid->addWidget(log, 1);

    auto *glossary = new QLabel(QString::fromUtf8(
        "<b>СМО</b> - система массового обслуживания<br>"
        "<b>И1 темп</b> - датчик температуры, нагрев/охлаждение<br>"
        "<b>И2 влажн</b> - датчик влажности, полив/вентиляция<br>"
        "<b>И3 свет</b> - датчик света, UV-лампы или зонты<br>"
        "<b>И4 вредит</b> - датчик вредителей, распылитель химикатов<br>"
        "<b>ДП</b> - диспетчер постановки в БП<br>"
        "<b>БП</b> - буферная память (очередь заявок)<br>"
        "<b>ДВ</b> - диспетчер выбора заявки и прибора<br>"
        "<b>Д2Б4</b> - приоритет источника, по одной заявке<br>"
        "<b>Д2П2</b> - указатель прибора по кольцу внутри группы<br>"
        "<b>P1, P2</b> - приборы температуры<br>"
        "<b>P3, P4</b> - приборы влажности<br>"
        "<b>P5, P6</b> - приборы света<br>"
        "<b>P7, P8</b> - приборы против вредителей<br>"
        "<b>оптимум / сейчас</b> - целевые и текущие ростовые условия<br>"
        "<b>команда</b> - актуатор после обслуживания заявки<br>"
        "<b>облака</b> - закрывают солнце и снижают освещённость<br>"
        "<b>req</b> - заявка, <b>p</b> - доля отказов, <b>N</b> - сколько заявок сгенерировано"));
    glossary->setWordWrap(true);
    glossary->setTextFormat(Qt::RichText);
    glossary->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    glossary->setMargin(8);
    auto *scroll = new QScrollArea;
    scroll->setWidget(glossary);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);

    auto *right = new QVBoxLayout;
    right->addWidget(pipeIn, 2);
    right->addWidget(pipeOut, 3);
    right->addWidget(pipeAll, 4);
    right->addWidget(new QLabel(QString::fromUtf8("Расшифровка сокращений")), 0);
    right->addWidget(scroll, 2);

    auto *columns = new QHBoxLayout;
    columns->addWidget(view, 3);
    auto *midBox = new QWidget;
    midBox->setLayout(mid);
    midBox->setMinimumWidth(340);
    columns->addWidget(midBox, 2);
    auto *rightBox = new QWidget;
    rightBox->setLayout(right);
    rightBox->setMinimumWidth(300);
    columns->addWidget(rightBox, 2);

    auto *root = new QVBoxLayout(&window);
    root->setContentsMargins(0, 0, 0, 0);
    root->addWidget(toolbar, 0);
    root->addLayout(columns, 1);

    QProcess proc;
    proc.setProcessChannelMode(QProcess::SeparateChannels);
    QByteArray pending;
    QList<QJsonObject> queue;
    QTimer play;
    play.setInterval(160);
    bool paused = false;
    bool splitScheme = true;
    double speed = 1.0;
    double climateTemp = launch.temp;
    double climateRh = launch.humidity;
    double climateLight = launch.light;
    double climateCo2 = launch.co2;
    double climatePests = launch.pests;
    int seed = launch.seed;

    auto applyFrame = [&](const QJsonObject &o) {
        view->frame = o;
        view->update();
        pipeIn->pulse(o);
        pipeOut->pulse(o);
        pipeAll->pulse(o);
        const auto buf = o.value("buffer").toArray();
        QStringList ids;
        for (const auto &v : buf)
            ids << v.toString();
        buffer->setText(ids.isEmpty() ? QString("-") : ids.join(", "));
        const auto dev = o.value("devices").toObject();
        QStringList dlines;
        const auto groups = o.value("groups").toArray();
        auto idleOf = [&](int id) {
            return dev.value(QString::number(id)).toString();
        };
        auto srcName = [](int src) {
            if (src == 1)
                return QString::fromUtf8("темп");
            if (src == 2)
                return QString::fromUtf8("влажн");
            if (src == 3)
                return QString::fromUtf8("свет");
            if (src == 4)
                return QString::fromUtf8("вредит");
            return QString("И%1").arg(src);
        };
        if (!groups.isEmpty()) {
            for (const auto &gv : groups) {
                const auto go = gv.toObject();
                const int src = go.value("source").toInt();
                QStringList parts;
                for (const auto &idv : go.value("devices").toArray()) {
                    const int id = idv.toInt();
                    parts << QString("P%1=%2").arg(id).arg(idleOf(id));
                }
                dlines << srcName(src) + ": " + parts.join("  ");
            }
        } else {
            for (auto it = dev.begin(); it != dev.end(); ++it)
                dlines << QString("P%1=%2").arg(it.key(), it.value().toString());
        }
        devices->setText(dlines.isEmpty() ? QString("-") : dlines.join("\n"));
        event->setText(QString::fromUtf8("step=%1  t=%2  %3  req=%4\nвыбрано=%5  Uk=P%6  p=%7  N=%8")
                           .arg(o.value("step").toInt())
                           .arg(o.value("time").toDouble())
                           .arg(o.value("event").toString())
                           .arg(o.value("request").toString())
                           .arg(o.value("selected").toString())
                           .arg(o.value("pointer").toInt())
                           .arg(o.value("p_reject").toDouble(), 0, 'f', 3)
                           .arg(o.value("generated").toInt()));
        log->appendPlainText(QString("t=%1 %2 req=%3 БП=[%4]")
                                 .arg(o.value("time").toDouble(), 0, 'f', 3)
                                 .arg(o.value("event").toString())
                                 .arg(o.value("request").toString())
                                 .arg(buffer->text()));
    };

    auto pull = [&]() {
        if (proc.state() == QProcess::Running)
            proc.write("\n");
    };

    QObject::connect(&play, &QTimer::timeout, [&]() {
        if (paused)
            return;
        if (queue.size() < 3)
            pull();
        if (queue.isEmpty())
            return;
        applyFrame(queue.takeFirst());
    });

    auto ingest = [&]() {
        pending += proc.readAllStandardOutput();
        while (true) {
            const int nl = pending.indexOf('\n');
            if (nl < 0)
                break;
            const QByteArray line = pending.left(nl).trimmed();
            pending.remove(0, nl + 1);
            if (line.isEmpty())
                continue;
            const auto doc = QJsonDocument::fromJson(line);
            if (doc.isObject())
                queue.append(doc.object());
        }
    };

    QObject::connect(&proc, &QProcess::readyReadStandardOutput, ingest);
    QObject::connect(&proc, &QProcess::readyReadStandardError, [&]() {
        const QString err = QString::fromUtf8(proc.readAllStandardError());
        if (!err.trimmed().isEmpty())
            log->appendPlainText(err.trimmed());
    });
    QObject::connect(&app, &QCoreApplication::aboutToQuit, [&]() {
        if (proc.state() != QProcess::NotRunning) {
            proc.kill();
            proc.waitForFinished(500);
        }
    });

    const auto updateSpeed = [&]() {
        play.setInterval(qBound(40, int(160.0 / speed), 800));
        speedLbl->setText(QString::fromUtf8("скорость x%1").arg(speed, 0, 'f', 2));
    };
    const auto slower = [&]() {
        speed = qMax(0.25, speed / 2.0);
        updateSpeed();
    };
    const auto faster = [&]() {
        speed = qMin(8.0, speed * 2.0);
        updateSpeed();
    };
    const auto togglePause = [&]() {
        paused = !paused;
        pauseBtn->setText(paused ? QString::fromUtf8("Продолжить (пробел)")
                                 : QString::fromUtf8("Пауза (пробел)"));
        if (!paused)
            play.start();
    };
    const auto toggleScheme = [&]() {
        splitScheme = !splitScheme;
        pipeIn->setVisible(splitScheme);
        pipeOut->setVisible(splitScheme);
        pipeAll->setVisible(!splitScheme);
        schemeBtn->setText(splitScheme ? QString::fromUtf8("Схема: один блок")
                                       : QString::fromUtf8("Схема: два блока"));
    };

    const auto startSim = [&]() {
        if (proc.state() != QProcess::NotRunning) {
            proc.kill();
            proc.waitForFinished(1500);
        }
        queue.clear();
        pending.clear();
        log->clear();
        const QDir binDir(QCoreApplication::applicationDirPath());
        QString smo = launch.smoPath;
        if (smo.isEmpty())
            smo = QDir::cleanPath(binDir.absoluteFilePath("../smo.py"));
        else if (QDir::isRelativePath(smo))
            smo = QDir::cleanPath(QDir::current().absoluteFilePath(smo));
        if (!QFileInfo::exists(smo)) {
            event->setText(QString::fromUtf8(
                               "Нет скрипта %1. В корне репозитория: chmod +x smo.py && python3 ./smo.py decode")
                               .arg(smo));
            return;
        }
        proc.setWorkingDirectory(QDir::cleanPath(binDir.absoluteFilePath("..")));
        proc.setProgram(smo);
        QStringList args{
            QStringLiteral("step"),
            QStringLiteral("--json-lines"),
            QStringLiteral("--pull"),
            QStringLiteral("--sources"), QString::number(launch.sources),
            QStringLiteral("--devices"), QString::number(launch.devices),
            QStringLiteral("--buffer"), QString::number(launch.buffer),
            QStringLiteral("--seed"), QString::number(seed),
            QStringLiteral("--mu"), QString::number(launch.mu, 'f', 4),
            QStringLiteral("-a"), QString::number(launch.a, 'f', 3),
            QStringLiteral("-b"), QString::number(launch.b, 'f', 3),
            QStringLiteral("--buffer-dwell"), QString::number(launch.bufferDwell, 'f', 3),
            QStringLiteral("--temp"), QString::number(climateTemp, 'f', 1),
            QStringLiteral("--humidity"), QString::number(climateRh, 'f', 1),
            QStringLiteral("--light"), QString::number(climateLight, 'f', 1),
            QStringLiteral("--soil"), QString::number(launch.soil, 'f', 1),
            QStringLiteral("--co2"), QString::number(climateCo2, 'f', 1),
            QStringLiteral("--pests"), QString::number(climatePests, 'f', 1),
            QStringLiteral("--opt-temp"), QString::number(launch.optTemp, 'f', 1),
            QStringLiteral("--opt-humidity"), QString::number(launch.optHumidity, 'f', 1),
            QStringLiteral("--opt-light"), QString::number(launch.optLight, 'f', 1),
            QStringLiteral("--opt-pests"), QString::number(launch.optPests, 'f', 1),
        };
        if (launch.nRequests > 0)
            args << QStringLiteral("-n") << QString::number(launch.nRequests);
        else
            args << QStringLiteral("--infinite");
        proc.setArguments(args);
        event->setText(launch.nRequests > 0
                           ? QString::fromUtf8("Модель запущена, N=%1...").arg(launch.nRequests)
                           : QString::fromUtf8("Бесконечная модель запущена..."));
        proc.start();
        if (!proc.waitForStarted(3000)) {
            event->setText(QString::fromUtf8("Не удалось запустить python-скрипт ./smo.py"));
            return;
        }
        pull();
        if (!paused)
            play.start();
    };

    QObject::connect(pauseBtn, &QPushButton::clicked, togglePause);
    QObject::connect(slowerBtn, &QPushButton::clicked, slower);
    QObject::connect(fasterBtn, &QPushButton::clicked, faster);
    QObject::connect(schemeBtn, &QPushButton::clicked, toggleScheme);
    QObject::connect(weatherBtn, &QPushButton::clicked, [&]() {
        auto *rng = QRandomGenerator::global();
        auto uni = [&](double lo, double hi) { return lo + rng->generateDouble() * (hi - lo); };
        climateTemp = uni(10.0, 34.0);
        climateRh = uni(25.0, 90.0);
        climateLight = uni(5.0, 95.0);
        climateCo2 = uni(320.0, 900.0);
        climatePests = uni(4.0, 55.0);
        auto patch = [&](QJsonObject o) {
            auto cl = o.value("climate").toObject();
            cl["temperature"] = climateTemp;
            cl["humidity"] = climateRh;
            cl["light"] = climateLight;
            cl["co2"] = climateCo2;
            cl["pests"] = climatePests;
            o["climate"] = cl;
            return o;
        };
        view->frame = patch(view->frame);
        for (int i = 0; i < queue.size(); ++i)
            queue[i] = patch(queue[i]);
        view->update();
        if (proc.state() == QProcess::Running) {
            const QByteArray cmd = QString("WEATHER %1 %2 %3 %4 %5 %6\n")
                                       .arg(climateTemp, 0, 'f', 2)
                                       .arg(climateRh, 0, 'f', 2)
                                       .arg(climateLight, 0, 'f', 2)
                                       .arg(climateCo2, 0, 'f', 1)
                                       .arg(45.0, 0, 'f', 1)
                                       .arg(climatePests, 0, 'f', 1)
                                       .toUtf8();
            proc.write(cmd);
        }
    });
    QObject::connect(startBtn, &QPushButton::clicked, startSim);

    auto addShortcut = [&](const QKeySequence &seq, auto fn) {
        auto *sc = new QShortcut(seq, &window);
        sc->setContext(Qt::ApplicationShortcut);
        QObject::connect(sc, &QShortcut::activated, fn);
    };
    addShortcut(Qt::Key_Escape, [&]() { window.close(); });
    addShortcut(Qt::Key_Space, togglePause);
    addShortcut(Qt::Key_Minus, slower);
    addShortcut(Qt::Key_Plus, faster);
    addShortcut(Qt::Key_Equal, faster);

    updateSpeed();
    QTimer::singleShot(200, startSim);

    window.showMaximized();
    return app.exec();
}
