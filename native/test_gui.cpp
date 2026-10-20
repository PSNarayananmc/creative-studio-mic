#include "theme.h"
#include "window.h"
#include <QFile>
#include <QFileInfo>
#include <QJsonArray>
#include <QJsonDocument>
#include <QJsonObject>
#include <QTemporaryDir>
#include <QtTest/QtTest>
#include <cmath>
class StudioTest : public QObject {
    Q_OBJECT
  private slots:
    void recordingEditsExports() {
        QTemporaryDir temp;
        QVERIFY(temp.isValid());
        qputenv("PULSE_SERVER", ("unix:" + temp.path() + "/pulse.sock").toUtf8());
        qputenv("XDG_CONFIG_HOME", (temp.path() + "/config").toUtf8());
        QProcess pulse;
        pulse.start("pulseaudio",
                    {"-n", "--daemonize=no", "--exit-idle-time=-1", "--log-target=stderr",
                     "--load=module-native-protocol-unix socket=" + temp.path() +
                         "/pulse.sock auth-anonymous=1"});
        QVERIFY(pulse.waitForStarted());
        QTest::qWait(500);
        pulseCommand({"load-module", "module-null-sink", "sink_name=gui_test_input", "rate=48000",
                      "channels=1"});
        pulseCommand({"load-module", "module-remap-source", "master=gui_test_input.monitor",
                      "source_name=gui_test_mic", "channels=1"});
        QFile tone(temp.path() + "/tone.f32");
        QVERIFY(tone.open(QIODevice::WriteOnly));
        for (int i = 0; i < 48000 * 20; i++) {
            float x = .12f * std::sin(i * 2 * M_PI * 220 / 48000);
            tone.write(reinterpret_cast<const char *>(&x), 4);
        }
        tone.close();
        QProcess producer;
        producer.setStandardInputFile(tone.fileName());
        producer.start("pacat", {"--playback", "--device=gui_test_input", "--raw", "--rate=48000",
                                 "--channels=1", "--format=float32le", "--latency-msec=20"});
        QVERIFY(producer.waitForStarted());
        applyTheme(*qobject_cast<QApplication *>(QCoreApplication::instance()));
        Window w;
        w.show();
        w.audio.start("gui_test_mic");
        w.startButton->setText("STOP");
        QTRY_VERIFY_WITH_TIMEOUT(w.audio.worker && w.audio.worker->ready, 6000);
        QTest::qWait(500);
        w.beginRecord();
        QVERIFY(w.recording);
        QTRY_VERIFY_WITH_TIMEOUT(w.recordedSamples > 72000, 8000);
        QTest::mouseClick(w.pauseButton, Qt::LeftButton);
        QVERIFY(w.paused);
        QTest::qWait(100);
        qint64 atPause = w.recordedSamples;
        QTest::qWait(400);
        QCOMPARE(w.recordedSamples, atPause);
        QTest::mouseClick(w.pauseButton, Qt::LeftButton);
        QTRY_VERIFY_WITH_TIMEOUT(w.recordedSamples > 144000, 8000);
        w.finishRecord();
        QVERIFY(!w.recording);
        QVERIFY(w.recordedSamples > 48000);
        QVERIFY(w.recordedSamples < 48000 * 6);
        w.trimStart->setValue(.1);
        w.trimEnd->setValue(1.5);
        w.edit(false);
        QCOMPARE(w.segments.size(), 1);
        QCOMPARE(w.segments[0].last - w.segments[0].first, qint64(67200));
        w.splitPoint->setValue(.5);
        w.edit(true);
        QCOMPARE(w.segments.size(), 2);
        for (auto *b : w.findChildren<QPushButton *>())
            if (b->text() == "Undo")
                QTest::mouseClick(b, Qt::LeftButton);
        QCOMPARE(w.segments.size(), 1);
        for (auto *b : w.findChildren<QPushButton *>())
            if (b->text() == "Redo")
                QTest::mouseClick(b, Qt::LeftButton);
        QCOMPARE(w.segments.size(), 2);
        for (int format = 0; format < 3; format++) {
            w.exportFormat->setCurrentIndex(format);
            w.exportChannels->setCurrentIndex(format == 1 ? 1 : 0);
            QString path = temp.path() + "/export." + QStringList{"wav", "flac", "mp3"}[format];
            w.exportAudio(false, path);
            QTRY_COMPARE_WITH_TIMEOUT(w.exportProcess.state(), QProcess::NotRunning, 10000);
            QCOMPARE(w.exportProcess.exitCode(), 0);
            QVERIFY(QFileInfo(path).size() > 1000);
            QProcess probe;
            probe.start("ffprobe", {"-v", "error", "-show_entries",
                                    "stream=sample_rate,channels,duration", "-of", "json", path});
            QVERIFY(probe.waitForFinished());
            auto stream = QJsonDocument::fromJson(probe.readAllStandardOutput())["streams"]
                              .toArray()[0]
                              .toObject();
            QCOMPARE(stream["channels"].toInt(), format == 1 ? 2 : 1);
            QCOMPARE(stream["sample_rate"].toString(), QString("48000"));
        }
        w.grab().save(qEnvironmentVariable("VC_EVIDENCE_DIR",QDir::currentPath()+"/evidence")+"/studio.png");
        QFile stat("/proc/self/status");
        QVERIFY(stat.open(QIODevice::ReadOnly));
        QByteArray rss;
        for (auto line : stat.readAll().split('\n'))
            if (line.startsWith("VmRSS:"))
                rss = line;
        QFile result(qEnvironmentVariable("VC_EVIDENCE_DIR",QDir::currentPath()+"/evidence")+"/gui-results.json");
        QVERIFY(result.open(QIODevice::WriteOnly));
        result.write(
            QJsonDocument(QJsonObject{{"recorded_samples", double(w.recordedSamples)},
                                      {"trimmed_samples", 67200},
                                      {"segments", w.segments.size()},
                                      {"exports", "WAV mono, FLAC stereo, MP3 mono at 48 kHz"},
                                      {"rss", QString::fromUtf8(rss)}})
                .toJson());
        w.audio.stop();
        QVERIFY(!pulseCommand({"list", "short", "modules"}).contains("voicecraft_"));
        w.close();
        producer.terminate();
        producer.waitForFinished();
        pulse.terminate();
        pulse.waitForFinished();
    }
};
QTEST_MAIN(StudioTest)
#include "test_gui.moc"
