#pragma once

#include "types.h"

#include <QHash>
#include <QJsonObject>
#include <QList>
#include <QString>
#include <QVector>
#include <QWidget>

class PipelineView : public QWidget
{
public:
    PipeKind kind = PipeKind::Combined;
    QJsonObject frame;
    QVector<Fly> flies;
    QString hot;
    QHash<int, QString> lastOcc;

    explicit PipelineView(PipeKind k, QWidget *parent = nullptr);
    void pulse(const QJsonObject &o);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    struct MetricGroup {
        int source = 0;
        QList<int> ids;
        QList<int> sources;
    };

    QList<MetricGroup> metricGroups() const;
    QStringList deviceNodeIds() const;
    QStringList sourceNodeIds() const;
    QHash<QString, QRect> nodeRects() const;
    void spawnFlies(const QString &req, const QList<int> &newlyBusy);
};
