#pragma once

#include <QCoreApplication>
#include <QString>

struct SimLaunch {
    int sources = -1;
    int sourcesTemp = -1;
    int sourcesHumidity = -1;
    int sourcesLight = -1;
    int sourcesPests = -1;
    int devices = -1;
    int devicesTemp = -1;
    int devicesHumidity = -1;
    int devicesLight = -1;
    int devicesPests = -1;
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

SimLaunch parseLaunch(QCoreApplication &app);
int runGreenhouseWindow(int argc, char *argv[]);
