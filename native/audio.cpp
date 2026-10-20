#include "audio.h"
#include <QElapsedTimer>
#include <QMutexLocker>
#include <QProcess>
#include <QProcessEnvironment>
#include <QUuid>
#include <cmath>
#include <cstring>
#include <stdexcept>
QString pulseCommand(const QStringList &args, int timeout) {
    QProcess p;
    auto env = QProcessEnvironment::systemEnvironment();
    env.insert("LC_ALL", "C");
    p.setProcessEnvironment(env);
    p.start("pactl", args);
    if (!p.waitForStarted(timeout) || !p.waitForFinished(timeout)) {
        p.kill();
        p.waitForFinished();
        throw std::runtime_error(
            "Audio server command timed out. Check PulseAudio/pipewire-pulse.");
    }
    if (p.exitCode() != 0)
        throw std::runtime_error(p.readAllStandardError().trimmed().toStdString());
    return QString::fromUtf8(p.readAllStandardOutput());
}
QVector<Device> microphones() {
    QVector<Device> result;
    QString name, description;
    bool monitor = false;
    auto append = [&]() {
        if (!name.isEmpty() && !monitor && !name.contains(".monitor") &&
            !name.startsWith("voicecraft_source_") && !name.startsWith("creative_mic_source_"))
            result.push_back({name, description.isEmpty() ? name : description});
    };
    for (QString line : pulseCommand({"list", "sources"}).split('\n')) {
        if (line.startsWith("Source #")) {
            append();
            name.clear();
            description.clear();
            monitor = false;
        }
        line = line.trimmed();
        if (line.startsWith("Name: "))
            name = line.mid(6);
        if (line.startsWith("Description: "))
            description = line.mid(13);
        if (line.startsWith("Monitor of Sink: ") && !line.endsWith("n/a"))
            monitor = true;
    }
    append();
    return result;
}
void AudioWorker::setSettings(const vc::Settings &s) {
    QMutexLocker lock(&settingsMutex);
    pending = s;
}
void AudioWorker::capture(QVector<float> &raw, QVector<float> &wet) {
    uint64_t r = captureRead.load(), w = captureWrite.load(std::memory_order_acquire);
    auto n = std::min<uint64_t>(w - r, CaptureSize);
    raw.resize(int(n));
    wet.resize(int(n));
    for (unsigned i = 0; i < n; i++) {
        raw[int(i)] = captureRaw[(r + i) % CaptureSize];
        wet[int(i)] = captureWet[(r + i) % CaptureSize];
    }
    captureRead.store(r + n, std::memory_order_release);
}
void AudioWorker::receive() {
    const void *data = nullptr;
    size_t bytes = 0;
    if (pa_stream_peek(input, &data, &bytes) < 0)
        return;
    const float *samples = static_cast<const float *>(data);
    for (size_t j = 0; j < bytes / sizeof(float); j++) {
        frame[filled++] = samples ? samples[j] : 0;
        if (filled != 480)
            continue;
        filled = 0;
        QElapsedTimer clock;
        clock.start();
        auto m = dsp.process(frame.data(), processed.data(), active);
        float elapsed = clock.nsecsElapsed() / 100000.f;
        dspLoad.store(.95f * dspLoad.load() + .05f * elapsed);
        inputLevel = m.input;
        outputLevel = m.output;
        reduction = m.reduction;
        noise = m.noise;
        if (m.clipped)
            clipping++;
        for (unsigned i = 0; i < 128; i++)
            waveform[i] = processed[i * 480 / 128];
        if (writePos - readPos + 480 > fifo.size()) {
            overruns++;
            readPos = writePos;
        } // bounded recovery, never grow buffers
        for (float x : processed)
            fifo[writePos++ % fifo.size()] = x;
        if (recording.load()) {
            uint64_t w = captureWrite.load(), r = captureRead.load(std::memory_order_acquire);
            if (w - r + 480 <= CaptureSize) {
                for (int i = 0; i < 480; i++) {
                    captureRaw[(w + i) % CaptureSize] = frame[i];
                    captureWet[(w + i) % CaptureSize] = processed[i];
                }
                captureWrite.store(w + 480, std::memory_order_release);
            } else {
                captureOverrun = true;
                recording = false;
            }
        }
        frames++;
    }
    pa_stream_drop(input);
}
void AudioWorker::deliver(size_t bytes) {
    float block[480];
    size_t count = bytes / sizeof(float);
    bool missed = false;
    while (count) {
        size_t n = std::min<size_t>(480, count);
        for (size_t i = 0; i < n; i++) {
            if (readPos < writePos)
                block[i] = fifo[readPos++ % fifo.size()];
            else {
                block[i] = 0;
                missed = true;
            }
        }
        if (pa_stream_write(output, block, n * sizeof(float), nullptr, 0, PA_SEEK_RELATIVE) < 0) {
            error = "Audio playback failed";
            stopRequested = true;
            break;
        }
        count -= n;
    }
    if (missed && ready)
        underruns++;
}
bool AudioWorker::healthy() const {
    return context && pa_context_get_state(context) != PA_CONTEXT_FAILED &&
           pa_context_get_state(context) != PA_CONTEXT_TERMINATED;
}
void AudioWorker::run() {
    loop = pa_mainloop_new();
    if (!loop) {
        error = "Cannot create audio loop";
        return;
    }
    context = pa_context_new(pa_mainloop_get_api(loop), "VoiceCraft Studio");
    if (!context || pa_context_connect(context, nullptr, PA_CONTEXT_NOAUTOSPAWN, nullptr) < 0) {
        error = "Cannot connect to audio server";
        goto cleanup;
    }
    {
        QElapsedTimer deadline;
        deadline.start();
        while (!stopRequested && healthy() && pa_context_get_state(context) != PA_CONTEXT_READY &&
               deadline.elapsed() < 5000) {
            pa_mainloop_iterate(loop, 0, nullptr);
            msleep(2);
        }
        if (stopRequested)
            goto cleanup;
        if (!healthy() || pa_context_get_state(context) != PA_CONTEXT_READY) {
            error = "Audio server unavailable";
            goto cleanup;
        }
    }
    {
        pa_sample_spec spec{PA_SAMPLE_FLOAT32NE, 48000, 1};
        input = pa_stream_new(context, "VoiceCraft microphone input", &spec, nullptr);
        output = pa_stream_new(context, "VoiceCraft processed output", &spec, nullptr);
        if (!input || !output) {
            error = "Cannot allocate audio streams";
            goto cleanup;
        }
        pa_stream_set_read_callback(
            input,
            [](pa_stream *, size_t, void *self) { static_cast<AudioWorker *>(self)->receive(); },
            this);
        pa_stream_set_write_callback(
            output,
            [](pa_stream *, size_t n, void *self) { static_cast<AudioWorker *>(self)->deliver(n); },
            this);
        pa_buffer_attr attr;
        attr.maxlength = 48000 * sizeof(float) / 5;
        attr.tlength = 960 * sizeof(float);
        attr.prebuf = 0;
        attr.minreq = 480 * sizeof(float);
        attr.fragsize = 480 * sizeof(float);
        auto flags = static_cast<pa_stream_flags_t>(PA_STREAM_ADJUST_LATENCY | PA_STREAM_DONT_MOVE |
                                                    PA_STREAM_AUTO_TIMING_UPDATE |
                                                    PA_STREAM_INTERPOLATE_TIMING);
        if (pa_stream_connect_record(input, source.toUtf8().constData(), &attr, flags) < 0 ||
            pa_stream_connect_playback(output, sink.toUtf8().constData(), &attr, flags, nullptr,
                                       nullptr) < 0) {
            error = "Cannot open selected microphone or virtual output";
            goto cleanup;
        }
        QElapsedTimer deadline;
        deadline.start();
        while (!stopRequested) {
            {
                QMutexLocker lock(&settingsMutex);
                active = pending;
            }
            pa_mainloop_iterate(loop, 0, nullptr);
            if (!healthy() || pa_stream_get_state(input) == PA_STREAM_FAILED ||
                pa_stream_get_state(output) == PA_STREAM_FAILED ||
                pa_stream_get_state(input) == PA_STREAM_TERMINATED ||
                pa_stream_get_state(output) == PA_STREAM_TERMINATED) {
                error = "Audio device disconnected. Waiting to reconnect.";
                break;
            }
            if (pa_stream_get_state(input) == PA_STREAM_READY &&
                pa_stream_get_state(output) == PA_STREAM_READY) {
                ready = true;
                pa_usec_t in = 0, out = 0;
                int negative = 0;
                if (pa_stream_get_latency(input, &in, &negative) == 0 &&
                    pa_stream_get_latency(output, &out, &negative) == 0)
                    serverLatencyMs = float(in + out) / 1000.f;
            } else if (deadline.elapsed() > 5000) {
                error = "Audio stream startup timed out";
                break;
            }
            msleep(2);
        }
    }
cleanup:
    ready = false;
    if (input) {
        pa_stream_disconnect(input);
        pa_stream_unref(input);
        input = nullptr;
    }
    if (output) {
        pa_stream_disconnect(output);
        pa_stream_unref(output);
        output = nullptr;
    }
    if (context) {
        pa_context_disconnect(context);
        pa_context_unref(context);
        context = nullptr;
    }
    pa_mainloop_free(loop);
    loop = nullptr;
}
AudioController::AudioController(QObject *parent) : QObject(parent) {
    recovery.setInterval(2000);
    connect(&recovery, &QTimer::timeout, this, [this]() {
        if (!desired)
            return;
        if (worker && worker->ready) {
            if (!working) {
                working = true;
                emit status("READY FOR OBS");
                emit running(true);
            }
            return;
        }
        if (worker && worker->isRunning())
            return;
        teardown();
        emit status("Waiting for microphone / audio server. Retrying…");
        launch();
    });
}
AudioController::~AudioController() {
    stop();
}
void AudioController::setSettings(const vc::Settings &s) {
    settings = s;
    if (worker)
        worker->setSettings(s);
}
void AudioController::teardown() {
    emit running(false);
    if (worker) {
        worker->recording = false;
        worker->stopRequested = true;
        worker->wait();
        delete worker;
        worker = nullptr;
    }
    working = false;
    // IDs may be reused after a server restart: verify token ownership before unload.
    try {
        QString listing = pulseCommand({"list", "short", "modules"});
        for (const QString &id : modules) {
            for (QString row : listing.split('\n'))
                if (row.section('\t', 0, 0) == id && row.contains(token))
                    pulseCommand({"unload-module", id});
        }
    } catch (const std::exception &) {
    }
    modules.clear();
    emit running(false);
}
void AudioController::launch() {
    try {
        auto devices = microphones();
        bool exists = false;
        for (auto &d : devices)
            exists |= d.name == selected;
        if (!exists)
            throw std::runtime_error(
                "Selected microphone is disconnected. Reconnect it or select another device.");
        token = QUuid::createUuid().toString(QUuid::Id128);
        sinkName = "voicecraft_sink_" + token;
        sourceName = "voicecraft_source_" + token;
        modules << pulseCommand({"load-module", "module-null-sink", "sink_name=" + sinkName,
                                 "rate=48000", "channels=1",
                                 "sink_properties=device.description=VoiceCraft_Internal"})
                       .trimmed();
        modules << pulseCommand(
                       {"load-module", "module-remap-source", "master=" + sinkName + ".monitor",
                        "source_name=" + sourceName, "channels=1",
                        "source_properties='device.description=\"VoiceCraft Studio Microphone\"'"})
                       .trimmed();
        worker = new AudioWorker(this);
        worker->source = selected;
        worker->sink = sinkName;
        worker->setSettings(settings);
        worker->start();
        emit status("Connecting audio streams…");
    } catch (const std::exception &e) {
        teardown();
        emit status(QString::fromUtf8(e.what()));
    }
}
void AudioController::start(const QString &source) {
    stop();
    if (source.isEmpty()) {
        emit status("Select a microphone first.");
        return;
    }
    selected = source;
    desired = true;
    launch();
    recovery.start();
}
void AudioController::stop() {
    desired = false;
    recovery.stop();
    teardown();
    emit status("STOPPED");
}
void AudioController::repair() {
    if (!desired)
        return;
    teardown();
    launch();
}
