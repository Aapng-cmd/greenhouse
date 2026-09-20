#include "sim_launch.h"

#include "greenhouse_view.h"
#include "pipeline_view.h"
#include "types.h"

#include <QtWidgets>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QProcess>
#include <QShortcut>

int runGreenhouseWindow(int argc, char *argv[])
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
    auto *midBox = new QWidget;
    midBox->setLayout(mid);
    midBox->setMinimumWidth(160);

    auto *glossary = new QLabel;
    glossary->setWordWrap(true);
    glossary->setTextFormat(Qt::RichText);
    glossary->setAlignment(Qt::AlignTop | Qt::AlignLeft);
    glossary->setMargin(8);
    auto *scroll = new QScrollArea;
    scroll->setWidget(glossary);
    scroll->setWidgetResizable(true);
    scroll->setFrameShape(QFrame::NoFrame);
    scroll->setMinimumHeight(80);

    const QString glossaryHead = QString::fromUtf8(
        "<b>СМО</b> - система массового обслуживания<br>"
        "<b>И</b> - источник (датчик категории)<br>"
        "<b>ДП</b> - диспетчер постановки в БП<br>"
        "<b>БП</b> - буферная память (очередь заявок)<br>"
        "<b>ДВ</b> - диспетчер выбора заявки и прибора<br>"
        "<b>Д2Б4</b> - приоритет источника, по одной заявке<br>"
        "<b>Д2П2</b> - указатель прибора по кольцу внутри группы<br>");
    const QString glossaryTail = QString::fromUtf8(
        "<b>оптимум / сейчас</b> - целевые и текущие ростовые условия<br>"
        "<b>команда</b> - актуатор после обслуживания заявки<br>"
        "<b>облака</b> - закрывают солнце и снижают освещённость<br>"
        "<b>req</b> - заявка, <b>p</b> - доля отказов, <b>N</b> - сколько заявок сгенерировано");
    glossary->setText(glossaryHead + glossaryTail);

    auto *glossBox = new QWidget;
    auto *glossLay = new QVBoxLayout(glossBox);
    glossLay->setContentsMargins(0, 0, 0, 0);
    glossLay->addWidget(new QLabel(QString::fromUtf8("Расшифровка сообщений")));
    glossLay->addWidget(scroll, 1);

    auto *rightSplit = new QSplitter(Qt::Vertical);
    rightSplit->addWidget(pipeIn);
    rightSplit->addWidget(pipeOut);
    rightSplit->addWidget(pipeAll);
    rightSplit->addWidget(glossBox);
    rightSplit->setStretchFactor(0, 2);
    rightSplit->setStretchFactor(1, 3);
    rightSplit->setStretchFactor(2, 4);
    rightSplit->setStretchFactor(3, 2);
    rightSplit->setChildrenCollapsible(true);

    view->setMinimumWidth(160);

    auto *splitMain = new QSplitter(Qt::Horizontal);
    splitMain->addWidget(view);
    splitMain->addWidget(rightSplit);
    splitMain->addWidget(midBox);
    splitMain->setStretchFactor(0, 3);
    splitMain->setStretchFactor(1, 2);
    splitMain->setStretchFactor(2, 2);
    splitMain->setChildrenCollapsible(true);
    splitMain->setSizes({520, 400, 340});

    auto *root = new QVBoxLayout(&window);
    root->setContentsMargins(0, 0, 0, 0);
    root->addWidget(toolbar, 0);
    root->addWidget(splitMain, 1);

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
        auto srcName = [](int src) { return groupTitle(src); };
        if (!groups.isEmpty()) {
            for (const auto &gv : groups) {
                const auto go = gv.toObject();
                const int src = go.contains("category") ? go.value("category").toInt()
                                                       : go.value("source").toInt();
                QStringList parts;
                QStringList srcs;
                for (const auto &idv : go.value("sources").toArray())
                    srcs << QString("И%1").arg(idv.toInt());
                for (const auto &idv : go.value("devices").toArray()) {
                    const int id = idv.toInt();
                    parts << QString("P%1=%2").arg(id).arg(idleOf(id));
                }
                QString head = srcName(src);
                if (!srcs.isEmpty())
                    head += " [" + srcs.join(" ") + "]";
                dlines << head + ": " + parts.join("  ");
            }
        } else {
            for (auto it = dev.begin(); it != dev.end(); ++it)
                dlines << QString("P%1=%2").arg(it.key(), it.value().toString());
        }
        devices->setText(dlines.isEmpty() ? QString("-") : dlines.join("\n"));
        QString actsHtml;
        const auto acts = o.value("actuators").toArray();
        if (!acts.isEmpty()) {
            QMap<int, QMap<QString, QList<int>>> grouped;
            QHash<QString, double> deltas;
            QHash<QString, QString> names;
            for (const auto &v : acts) {
                const auto a = v.toObject();
                const int cat = a.value("category").toInt();
                const QString name = a.value("name").toString();
                const double dlt = a.value("delta").toDouble();
                const QString key = name + QChar(0x1f) + QString::number(dlt, 'f', 2);
                grouped[cat][key] << a.value("id").toInt();
                deltas.insert(key, dlt);
                names.insert(key, name);
            }
            QStringList lines;
            for (auto catIt = grouped.begin(); catIt != grouped.end(); ++catIt) {
                const auto &byType = catIt.value();
                for (auto typeIt = byType.begin(); typeIt != byType.end(); ++typeIt) {
                    QStringList ids;
                    for (int id : typeIt.value())
                        ids << QString("P%1").arg(id);
                    const double dlt = deltas.value(typeIt.key());
                    QString dltS = QString::number(dlt, 'f', 2);
                    if (dlt >= 0.0)
                        dltS.prepend(QLatin1Char('+'));
                    lines << ids.join(", ") + " - " + names.value(typeIt.key()) + ", delta = " + dltS;
                }
            }
            actsHtml = QString::fromUtf8("<br><b>Приборы</b><br>") + lines.join("<br>") + "<br>";
        }
        glossary->setText(glossaryHead + actsHtml + glossaryTail);
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
        auto addCount = [&](int value, const char *flag) {
            if (value >= 0)
                args << QString::fromUtf8(flag) << QString::number(value);
        };
        addCount(launch.sources, "--sources");
        addCount(launch.sourcesTemp, "--sources-temp");
        addCount(launch.sourcesHumidity, "--sources-humidity");
        addCount(launch.sourcesLight, "--sources-light");
        addCount(launch.sourcesPests, "--sources-pests");
        addCount(launch.devices, "--devices");
        addCount(launch.devicesTemp, "--devices-temp");
        addCount(launch.devicesHumidity, "--devices-humidity");
        addCount(launch.devicesLight, "--devices-light");
        addCount(launch.devicesPests, "--devices-pests");
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
