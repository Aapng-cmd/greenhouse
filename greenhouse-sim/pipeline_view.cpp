#include "pipeline_view.h"

#include <QJsonArray>
#include <QPaintEvent>
#include <QPainter>
#include <QTimer>

PipelineView::PipelineView(PipeKind k, QWidget *parent)
    : QWidget(parent), kind(k)
{
    setMinimumSize(120, 80);
    setAttribute(Qt::WA_OpaquePaintEvent);
    auto *tick = new QTimer(this);
    connect(tick, &QTimer::timeout, this, [this]() {
        if (flies.isEmpty())
            return;
        for (int i = flies.size() - 1; i >= 0; --i) {
            flies[i].u += 0.04;
            if (flies[i].u >= 1.0)
                flies.removeAt(i);
        }
        update();
    });
    tick->start(40);
}

void PipelineView::pulse(const QJsonObject &o)
{
    const auto prevOcc = lastOcc;
    frame = o;
    lastOcc.clear();
    QList<int> newlyBusy;
    const auto occ = o.value("devices").toObject();
    for (auto it = occ.begin(); it != occ.end(); ++it) {
        const int id = it.key().toInt();
        const QString now = it.value().toString();
        lastOcc.insert(id, now);
        if (!now.isEmpty() && now != QLatin1String("idle") && now != prevOcc.value(id))
            newlyBusy.append(id);
    }

    const QString ev = o.value("event").toString();
    const bool inboundEv = ev.contains(QString::fromUtf8("сгенерирована"))
                           || ev.contains(QString::fromUtf8("выбивание"));
    const bool outboundEv = ev.contains(QString::fromUtf8("обработка"))
                            || ev.contains(QString::fromUtf8("отправка"))
                            || ev.contains(QString::fromUtf8("прибор"))
                            || ev.contains(QString::fromUtf8("выдача"))
                            || !newlyBusy.isEmpty();
    if (kind == PipeKind::Inbound && !inboundEv)
        return;
    if (kind == PipeKind::Outbound && !outboundEv)
        return;
    if (inboundEv)
        hot = QStringLiteral("src");
    else if (outboundEv)
        hot = QStringLiteral("dv");
    else
        hot = QStringLiteral("buf");
    spawnFlies(o.value("request").toString(), newlyBusy);
    update();
}

void PipelineView::paintEvent(QPaintEvent *)
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
        for (const auto &id : sourceNodeIds())
            drawEdge(id, "dp");
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
    drawNode("dp", QString::fromUtf8("ДП"), QColor(90, 90, 140));
    drawNode("buf", QString::fromUtf8("БП"), QColor(80, 130, 90));
    drawNode("dv", QString::fromUtf8("ДВ"), QColor(90, 90, 140));
    p.setFont(QFont("Sans", 7));
    for (const auto &g : metricGroups()) {
        if (g.sources.isEmpty())
            continue;
        const QString first = QStringLiteral("s%1").arg(g.sources.first());
        const QString last = QStringLiteral("s%1").arg(g.sources.last());
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
        for (int sid : g.sources)
            drawNode(QStringLiteral("s%1").arg(sid), QStringLiteral("И%1").arg(sid), col);
    }
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
        p.drawEllipse(pos, 7, 7);
    }
}

QList<PipelineView::MetricGroup> PipelineView::metricGroups() const
{
    QList<MetricGroup> out;
    const auto arr = frame.value("groups").toArray();
    if (!arr.isEmpty()) {
        for (const auto &v : arr) {
            const auto o = v.toObject();
            MetricGroup g;
            g.source = o.value("category").toInt();
            if (g.source == 0)
                g.source = o.value("source").toInt();
            for (const auto &id : o.value("devices").toArray())
                g.ids.append(id.toInt());
            for (const auto &id : o.value("sources").toArray())
                g.sources.append(id.toInt());
            if (g.sources.isEmpty() && g.source > 0)
                g.sources.append(g.source);
            if (!g.ids.isEmpty() || !g.sources.isEmpty())
                out.append(g);
        }
        return out;
    }
    for (int s = 1; s <= 4; ++s)
        out.append({s, {2 * s - 1, 2 * s}, {s}});
    return out;
}

QStringList PipelineView::deviceNodeIds() const
{
    QStringList ids;
    for (const auto &g : metricGroups()) {
        for (int id : g.ids)
            ids << QStringLiteral("p%1").arg(id);
    }
    return ids;
}

QStringList PipelineView::sourceNodeIds() const
{
    QStringList ids;
    for (const auto &g : metricGroups()) {
        for (int id : g.sources)
            ids << QStringLiteral("s%1").arg(id);
    }
    return ids;
}

QHash<QString, QRect> PipelineView::nodeRects() const
{
    QHash<QString, QRect> m;
    const int w = 54, h = 26;
    const int cx = width() / 2;
    const int top = 24;
    auto placeSources = [&](int y0) {
        const auto groups = metricGroups();
        const int cols = qMax(1, groups.size());
        const int boxW = 40;
        const int boxH = 24;
        const int vGap = 6;
        const int avail = qMax(boxW, width() - 16);
        const int colGap = cols > 1 ? qMax(14, (avail - cols * boxW) / (cols - 1)) : 0;
        int bottom = y0;
        for (int c = 0; c < cols; ++c) {
            const auto &g = groups[c];
            const int x = 8 + c * (boxW + colGap);
            for (int i = 0; i < g.sources.size(); ++i) {
                const int y = y0 + i * (boxH + vGap);
                m[QStringLiteral("s%1").arg(g.sources[i])] = QRect(x, y, boxW, boxH);
                bottom = qMax(bottom, y + boxH);
            }
        }
        return bottom;
    };
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
        const int srcBottom = placeSources(top);
        m["dp"] = QRect(cx - w / 2, srcBottom + 16, w, h);
        m["buf"] = QRect(cx - 48, srcBottom + 58, 96, 34);
    } else if (kind == PipeKind::Outbound) {
        m["buf"] = QRect(cx - 48, top, 96, 34);
        m["dv"] = QRect(cx - w / 2, top + 46, w, h);
        placeDevices(top + 92);
    } else {
        const int srcBottom = placeSources(top);
        m["dp"] = QRect(cx - w / 2, srcBottom + 12, w, h);
        m["buf"] = QRect(cx - 50, srcBottom + 52, 100, 34);
        m["dv"] = QRect(cx - w / 2, srcBottom + 96, w, h);
        placeDevices(srcBottom + 136);
    }
    return m;
}

void PipelineView::spawnFlies(const QString &req, const QList<int> &newlyBusy)
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
    const auto occ = frame.value("devices").toObject();
    const QString selected = frame.value("selected").toString();
    QString want = req;
    if (want.isEmpty() || want == QLatin1String("-"))
        want = selected;
    if ((want.isEmpty() || want == QLatin1String("-")) && !newlyBusy.isEmpty())
        want = occ.value(QString::number(newlyBusy.first())).toString();

    int src = want.section(QLatin1Char('.'), 0, 0).toInt();
    if (src <= 0)
        src = req.section(QLatin1Char('.'), 0, 0).toInt();
    QString sid = QStringLiteral("s%1").arg(src > 0 ? src : 1);
    int cat = src;
    QList<int> groupIds;
    for (const auto &g : metricGroups()) {
        if (g.sources.contains(src) || (g.sources.isEmpty() && g.source == src) || g.source == src) {
            cat = g.source ? g.source : src;
            groupIds = g.ids;
            break;
        }
    }
    if (cat <= 0 && !newlyBusy.isEmpty()) {
        for (const auto &g : metricGroups()) {
            if (g.ids.contains(newlyBusy.first())) {
                cat = g.source;
                groupIds = g.ids;
                break;
            }
        }
    }
    const QColor col = sourceColor(cat > 0 ? cat : 1);
    if (kind == PipeKind::Inbound || (kind == PipeKind::Combined && hot == QLatin1String("src"))) {
        add(sid, QStringLiteral("dp"), col);
        add(QStringLiteral("dp"), QStringLiteral("buf"), col);
    }
    if (kind == PipeKind::Outbound || (kind == PipeKind::Combined && hot == QLatin1String("dv"))) {
        add(QStringLiteral("buf"), QStringLiteral("dv"), col);
        QList<int> assigned = newlyBusy;
        if (assigned.isEmpty() && !want.isEmpty() && want != QLatin1String("-")) {
            auto consider = [&](int id) {
                if (occ.value(QString::number(id)).toString() == want && !assigned.contains(id))
                    assigned << id;
            };
            for (int id : groupIds)
                consider(id);
            for (auto it = occ.begin(); it != occ.end(); ++it)
                consider(it.key().toInt());
        }
        for (int id : assigned)
            add(QStringLiteral("dv"), QStringLiteral("p%1").arg(id), col);
    }
    while (flies.size() > 28)
        flies.removeFirst();
}
