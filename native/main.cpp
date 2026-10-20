#include "theme.h"
#include "window.h"
#include <QApplication>
#include <QCommandLineParser>
#include <QDir>
#include <QFile>
#include <QJsonDocument>
#include <QJsonObject>
#include <QLockFile>
#include <QTimer>
#include <algorithm>
#include <cmath>
#include <csignal>
#include <cstdio>
#include <memory>
#include <unistd.h>
static volatile std::sig_atomic_t interrupted = 0;
static void signalHandler(int) {
    interrupted = 1;
}
int main(int argc, char **argv) {
    bool headless = false;
    for (int i = 1; i < argc; i++)
        if (QString::fromLocal8Bit(argv[i]) == "--headless" ||
            QString::fromLocal8Bit(argv[i]) == "--list-sources" ||
            QString::fromLocal8Bit(argv[i]) == "--version" ||
            QString::fromLocal8Bit(argv[i]) == "--help")
            headless = true;
    std::unique_ptr<QCoreApplication> app;
    if (headless)
        app = std::make_unique<QCoreApplication>(argc, argv);
    else
        app = std::make_unique<QApplication>(argc, argv);
    QCoreApplication::setApplicationName("voicecraft-studio");
    QCoreApplication::setApplicationVersion("0.1.0");
    QCommandLineParser parser;
    parser.setApplicationDescription("VoiceCraft Studio — offline native microphone processing");
    parser.addHelpOption();
    parser.addVersionOption();
    parser.addOption({"headless", "Run the native audio engine without GUI"});
    parser.addOption({"list-sources", "List selectable microphones"});
    parser.addOption({"source", "Exact microphone source name", "name"});
    parser.addOption({"seconds", "Stop after this many seconds (0 = continuous)", "seconds", "0"});
    parser.addOption(
        {"capture", "Record processed float32 mono PCM locally (explicit opt-in)", "path"});
    parser.addOption({"low-cpu", "Bypass neural suppression"});
    parser.addOption({"dry", "Aligned A/B bypass with safety limiting"});
    parser.addOption({"gui-smoke-test", "Open GUI, then close automatically"});
    parser.process(*app);
    if (parser.isSet("list-sources")) {
        try {
            for (auto &d : microphones())
                printf("%s\t%s\n", qPrintable(d.name), qPrintable(d.description));
            return 0;
        } catch (const std::exception &e) {
            fprintf(stderr, "%s\n", e.what());
            return 1;
        }
    }
    if (geteuid() == 0) {
        fprintf(stderr, "Run VoiceCraft Studio as a normal desktop user, never root.\n");
        return 1;
    }
    QLockFile lock(QDir::tempPath() + QString("/voicecraft-studio-%1.lock").arg(getuid()));
    lock.setStaleLockTime(0);
    if (!lock.tryLock(0)) {
        fprintf(stderr, "VoiceCraft Studio is already running for this user.\n");
        return 1;
    }
    std::signal(SIGINT, signalHandler);
    std::signal(SIGTERM, signalHandler);
    QTimer signalTimer;
    QObject::connect(&signalTimer, &QTimer::timeout, app.get(), [&]() {
        if (interrupted)
            app->quit();
    });
    signalTimer.start(50);
    if (parser.isSet("headless")) {
        AudioController audio;
        vc::Settings s;
        s.lowCpu = parser.isSet("low-cpu");
        s.bypass = parser.isSet("dry");
        audio.setSettings(s);
        QFile capture;
        bool failed = false;
        if (parser.isSet("capture")) {
            capture.setFileName(parser.value("capture"));
            if (!capture.open(QIODevice::WriteOnly | QIODevice::NewOnly)) {
                fprintf(stderr, "Cannot create capture file; existing files are preserved.\n");
                return 1;
            }
        }
        QObject::connect(&audio, &AudioController::status, app.get(), [&](const QString &state) {
            QJsonObject o{{"status", state}, {"source", audio.virtualSource()}};
            auto line = QJsonDocument(o).toJson(QJsonDocument::Compact);
            fprintf(stderr, "%s\n", line.constData());
        });
        QTimer poll;
        QObject::connect(&poll, &QTimer::timeout, app.get(), [&]() {
            if (!audio.worker)
                return;
            auto *w = audio.worker;
            if (capture.isOpen()) {
                w->recording = true;
                QVector<float> raw, wet;
                w->capture(raw, wet);
                if (capture.write(reinterpret_cast<const char *>(wet.constData()),
                                  wet.size() * 4) != wet.size() * 4 ||
                    w->captureOverrun) {
                    failed = true;
                    app->quit();
                }
            }
        });
        poll.start(20);
        QTimer report;
        QObject::connect(&report, &QTimer::timeout, app.get(), [&]() {
            auto *w = audio.worker;
            if (!w)
                return;
            QJsonObject o{
                {"frames", double(w->frames.load())},
                {"input_dbfs", double(20 * std::log10(std::max(1e-6f, w->inputLevel.load())))},
                {"output_dbfs", double(20 * std::log10(std::max(1e-6f, w->outputLevel.load())))},
                {"dsp_percent", double(w->dspLoad.load())},
                {"buffer_ms", double(w->serverLatencyMs.load())},
                {"underruns", int(w->underruns.load())}};
            fprintf(stdout, "%s\n", QJsonDocument(o).toJson(QJsonDocument::Compact).constData());
            fflush(stdout);
        });
        report.start(1000);
        QString source = parser.value("source");
        if (source.isEmpty()) {
            fprintf(stderr, "Use --list-sources, then --source NAME.\n");
            return 2;
        }
        audio.start(source);
        bool valid = false;
        double seconds = parser.value("seconds").toDouble(&valid);
        if (!valid || !std::isfinite(seconds) || seconds < 0 || seconds > 86400)
            return 2;
        if (seconds > 0)
            QTimer::singleShot(int(seconds * 1000), app.get(), &QCoreApplication::quit);
        QObject::connect(app.get(), &QCoreApplication::aboutToQuit, &audio, [&]() {
            if (audio.worker) {
                audio.worker->recording = false;
                QVector<float> raw, wet;
                audio.worker->capture(raw, wet);
                if (capture.isOpen())
                    capture.write(reinterpret_cast<const char *>(wet.constData()), wet.size() * 4);
            }
            audio.stop();
        });
        int code = app->exec();
        return failed ? 1 : code;
    }
    QFile style(":/none");
    Q_UNUSED(style);
    applyTheme(*static_cast<QApplication *>(app.get()));
    Window window;
    window.show();
    if (parser.isSet("gui-smoke-test"))
        QTimer::singleShot(700, &window, &QWidget::close);
    return app->exec();
}
