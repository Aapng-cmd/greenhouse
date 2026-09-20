#pragma once

#include <QJsonObject>
#include <QMap>
#include <QPoint>
#include <QString>
#include <QVector>
#include <QWidget>

class GreenhouseView : public QWidget
{
public:
    QJsonObject frame;
    int phase = 0;
    QMap<QString, double> level;
    struct PlacedAct {
        int id = 0;
        QString action;
        QString name;
        QPoint pos;
        int variant = 0;
    };
    QVector<PlacedAct> placed;
    QString placedKey;

    explicit GreenhouseView(QWidget *parent = nullptr);

protected:
    void paintEvent(QPaintEvent *) override;

private:
    void rebuildPlaced(const QRect &house, const QRect &glass);
};
