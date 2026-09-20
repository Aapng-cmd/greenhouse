#include "sim_launch.h"

#include <QCommandLineParser>

SimLaunch parseLaunch(QCoreApplication &app)
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

    auto addOpt = [&](const QStringList &names, const char *help, const char *valueName) {
        QCommandLineOption opt(names, QString::fromUtf8(help), QString::fromUtf8(valueName));
        p.addOption(opt);
        return opt;
    };

    const auto sources = addOpt({"sources"}, "датчиков на каждую категорию", "N");
    const auto sourcesTemp = addOpt({"sources-temp"}, "датчиков температуры (важнее --sources)", "N");
    const auto sourcesHumidity = addOpt({"sources-humidity"}, "датчиков влажности (важнее --sources)", "N");
    const auto sourcesLight = addOpt({"sources-light"}, "датчиков света (важнее --sources)", "N");
    const auto sourcesPests = addOpt({"sources-pests"}, "датчиков вредителей (важнее --sources)", "N");
    const auto devices = addOpt({"devices"}, "приборов на каждую категорию", "N");
    const auto devicesTemp = addOpt({"devices-temp"}, "приборов температуры (важнее --devices)", "N");
    const auto devicesHumidity = addOpt({"devices-humidity"}, "приборов влажности (важнее --devices)", "N");
    const auto devicesLight = addOpt({"devices-light"}, "приборов света (важнее --devices)", "N");
    const auto devicesPests = addOpt({"devices-pests"}, "приборов вредителей (важнее --devices)", "N");
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
    auto takeInt = [&](const QCommandLineOption &opt) {
        return p.isSet(opt) ? p.value(opt).toInt() : -1;
    };
    o.sources = takeInt(sources);
    o.sourcesTemp = takeInt(sourcesTemp);
    o.sourcesHumidity = takeInt(sourcesHumidity);
    o.sourcesLight = takeInt(sourcesLight);
    o.sourcesPests = takeInt(sourcesPests);
    o.devices = takeInt(devices);
    o.devicesTemp = takeInt(devicesTemp);
    o.devicesHumidity = takeInt(devicesHumidity);
    o.devicesLight = takeInt(devicesLight);
    o.devicesPests = takeInt(devicesPests);
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
