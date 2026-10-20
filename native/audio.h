#pragma once
#include "dsp.h"
#include <QMutex>
#include <QObject>
#include <QStringList>
#include <QThread>
#include <QTimer>
#include <QVector>
#include <array>
#include <atomic>
#include <pulse/pulseaudio.h>
struct Device {
    QString name, description;
};
QString pulseCommand(const QStringList &args, int timeout = 3000);
QVector<Device> microphones();
class AudioWorker : public QThread {
    Q_OBJECT
    vc::Processor dsp;
    QMutex settingsMutex;
    vc::Settings pending{}, active{};
    pa_mainloop *loop = nullptr;
    pa_context *context = nullptr;
    pa_stream *input = nullptr;
    pa_stream *output = nullptr;
    std::array<float, 480> frame{}, processed{};
    int filled = 0;
    std::array<float, 48000> fifo{};
    uint64_t readPos = 0, writePos = 0;
    static constexpr unsigned CaptureSize = 1 << 18;
    std::array<float, CaptureSize> captureRaw{}, captureWet{};
    std::atomic<uint64_t> captureRead{0}, captureWrite{0};
    void receive();
    void deliver(size_t bytes);
    bool healthy() const;

  protected:
    void run() override;

  public:
    explicit AudioWorker(QObject *parent = nullptr) : QThread(parent) {}
    QString source, sink, error;
    std::atomic<bool> stopRequested{false}, ready{false}, recording{false}, captureOverrun{false};
    std::atomic<float> inputLevel{0}, outputLevel{0}, reduction{0}, noise{-90}, dspLoad{0};
    std::atomic<unsigned> underruns{0}, clipping{0}, overruns{0};
    std::atomic<uint64_t> frames{0};
    std::array<std::atomic<float>, 128> waveform{};
    std::atomic<float> serverLatencyMs{-1};
    void setSettings(const vc::Settings &s);
    void capture(QVector<float> &raw, QVector<float> &wet);
    ~AudioWorker() override {
        stopRequested = true;
        wait();
    }
};
class AudioController : public QObject {
    Q_OBJECT
    QString token, sinkName, sourceName, selected;
    QStringList modules;
    QTimer recovery;
    bool desired = false, working = false;
    vc::Settings settings{};
    void teardown();
    void launch();

  public:
    AudioWorker *worker = nullptr;
    explicit AudioController(QObject *parent = nullptr);
    ~AudioController() override;
    void start(const QString &source);
    void stop();
    void repair();
    void setSettings(const vc::Settings &s);
    QString virtualSource() const {
        return sourceName;
    }
    bool wanted() const {
        return desired;
    }
  signals:
    void status(const QString &message);
    void running(bool ready);
};
