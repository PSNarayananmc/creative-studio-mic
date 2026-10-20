#pragma once
#include "audio.h"
#include "timeline.h"
#include <QCheckBox>
#include <QComboBox>
#include <QDoubleSpinBox>
#include <QLabel>
#include <QMainWindow>
#include <QMap>
#include <QProcess>
#include <QProgressBar>
#include <QPushButton>
#include <QSettings>
#include <QTemporaryFile>
#include <QTimer>
class Waveform : public QWidget {
    AudioController *audio;

  public:
    explicit Waveform(AudioController *c, QWidget *p = nullptr) : QWidget(p), audio(c) {
        setMinimumHeight(160);
    }

  protected:
    void paintEvent(QPaintEvent *) override;
};
class Window : public QMainWindow {
    Q_OBJECT
    friend class StudioTest;
    AudioController audio;
    QSettings preferences;
    QComboBox *devices, *presets;
    QPushButton *startButton, *recordButton, *pauseButton;
    QLabel *statusLabel, *audioStatus, *obsStatus, *meterText, *recordStatus, *analysisStatus;
    QProgressBar *inMeter, *outMeter;
    Waveform *waveform;
    RecordingTimeline *timeline;
    void refreshTimeline();
    QCheckBox *rawMode = nullptr, *lowCpu = nullptr, *agc = nullptr;
    QMap<QString, QDoubleSpinBox *> knobs;
    QTimer updateTimer;
    QTemporaryFile rawFile, wetFile;
    bool recording = false, paused = false, analyzing = false;
    unsigned analysisFrames = 0;
    double minRms = 1, sumPower = 0, lowPower = 0, highPower = 0, analysisPeak = 0, lastLow = 0;
    uint64_t analysisSamples = 0;
    struct Segment {
        qint64 first, last;
    };
    QVector<Segment> segments;
    QVector<QVector<Segment>> undoStack, redoStack;
    qint64 recordedSamples = 0;
    QDoubleSpinBox *trimStart, *trimEnd, *splitPoint;
    QComboBox *exportFormat, *exportChannels, *previewMode;
    QProcess exportProcess, previewProcess;
    QTemporaryFile exportData;
    void refreshDevices();
    void update();
    void apply();
    void loadPreset(int index);
    void beginRecord();
    void finishRecord();
    void edit(bool split);
    void exportAudio(bool preview = false, const QString &path = QString());
    void analyze();
    vc::Settings settings() const;
    QWidget *makeStudio();
    QWidget *makeRecording();
    QWidget *makeMixer();
    QWidget *makePresets();
    QWidget *makeSettings();
    QDoubleSpinBox *knob(const QString &key, double min, double max, double value, double step = 1,
                         const QString &suffix = "");

  protected:
    void closeEvent(QCloseEvent *) override;

  public:
    explicit Window();
};
