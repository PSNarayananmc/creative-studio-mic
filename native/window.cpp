#include "window.h"
#include <QApplication>
#include <QCloseEvent>
#include <QDateTime>
#include <QDir>
#include <QFileDialog>
#include <QGridLayout>
#include <QGroupBox>
#include <QHBoxLayout>
#include <QMessageBox>
#include <QPainter>
#include <QPainterPath>
#include <QScrollArea>
#include <QStandardPaths>
#include <QTabWidget>
#include <QVBoxLayout>
#include <algorithm>
#include <cmath>
static float dbfs(float x) {
    return 20 * std::log10(std::max(x, 1e-6f));
}
static QLabel *title(const QString &text) {
    auto *l = new QLabel(text);
    l->setObjectName("heading");
    return l;
}
void Waveform::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.setRenderHint(QPainter::Antialiasing);
    p.fillRect(rect(), QColor("#111520"));
    p.setPen(QColor("#242a3b"));
    for (int i = 1; i < 5; i++)
        p.drawLine(0, height() * i / 5, width(), height() * i / 5);
    for (int i = 1; i < 12; i++)
        p.drawLine(width() * i / 12, 0, width() * i / 12, height());
    QPainterPath path;
    path.moveTo(0, height() / 2);
    for (int i = 0; i < 128; i++) {
        float x = audio->worker ? audio->worker->waveform[i].load() : 0;
        path.lineTo(i * width() / 127., height() / 2 - x * height() * .43);
    }
    p.setPen(QPen(QColor("#a8f0c6"), 2));
    p.drawPath(path);
    p.setPen(QColor("#7d879c"));
    p.drawText(16, 24, "LIVE OUTPUT  ·  48 kHz / mono  ·  10 ms frames");
}
QDoubleSpinBox *Window::knob(const QString &key, double min, double max, double value, double step,
                             const QString &suffix) {
    auto *box = new QDoubleSpinBox;
    box->setRange(min, max);
    box->setSingleStep(step);
    box->setDecimals(step < 1 ? 2 : 1);
    box->setSuffix(suffix);
    box->setValue(value);
    knobs[key] = box;
    connect(box, QOverload<double>::of(&QDoubleSpinBox::valueChanged), this, [this]() { apply(); });
    return box;
}
Window::Window()
    : preferences("VoiceCraft", "Studio"), rawFile(QDir::tempPath() + "/voicecraft-raw-XXXXXX"),
      wetFile(QDir::tempPath() + "/voicecraft-wet-XXXXXX"),
      exportData(QDir::tempPath() + "/voicecraft-export-XXXXXX") {
    setWindowTitle("VoiceCraft Studio");
    setMinimumSize(920, 700);
    resize(1080, 820);
    auto *central = new QWidget;
    auto *layout = new QVBoxLayout(central);
    layout->setContentsMargins(28, 22, 28, 22);
    layout->setSpacing(16);
    auto *top = new QHBoxLayout;
    top->addWidget(title("VOICECRAFT  /  STUDIO"));
    top->addStretch();
    auto *version = new QLabel("0.1  •  OFFLINE AUDIO");
    version->setObjectName("muted");
    top->addWidget(version);
    layout->addLayout(top);
    auto *tabs = new QTabWidget;
    tabs->addTab(makeStudio(), "Studio");
    tabs->addTab(makeRecording(), "Recording");
    tabs->addTab(makeMixer(), "Mixer");
    tabs->addTab(makePresets(), "Presets");
    tabs->addTab(makeSettings(), "Settings");
    layout->addWidget(tabs);
    auto *footer = new QLabel(
        "In OBS, add Audio Input Capture (PulseAudio) and choose VoiceCraft Studio Microphone.");
    footer->setWordWrap(true);
    footer->setObjectName("muted");
    layout->addWidget(footer);
    setCentralWidget(central);
    connect(&audio, &AudioController::status, this,
            [this](const QString &s) { statusLabel->setText(s); });
    connect(&audio, &AudioController::running, this, [this](bool running) {
        if (!running && recording)
            finishRecord();
        if (!running && analyzing) {
            analyzing = false;
            analysisStatus->setText("Analysis canceled: audio connection changed.");
        }
    });
    connect(&updateTimer, &QTimer::timeout, this, &Window::update);
    updateTimer.start(40);
    connect(&exportProcess, QOverload<int, QProcess::ExitStatus>::of(&QProcess::finished), this,
            [this](int code, QProcess::ExitStatus) {
                recordStatus->setText(
                    code == 0
                        ? "Export complete."
                        : "Export failed: " +
                              QString::fromUtf8(exportProcess.readAllStandardError()).right(400));
            });
    connect(&exportProcess, &QProcess::errorOccurred, this, [this](QProcess::ProcessError) {
        recordStatus->setText("Export could not start. Install ffmpeg.");
    });
    refreshDevices();
    loadPreset(preferences.value("preset", 0).toInt());
    for (auto it = knobs.begin(); it != knobs.end(); ++it)
        if (preferences.contains("controls/" + it.key()))
            it.value()->setValue(preferences.value("controls/" + it.key()).toDouble());
    lowCpu->setChecked(preferences.value("lowCpu", false).toBool());
    apply();
}
QWidget *Window::makeStudio() {
    auto *page = new QWidget;
    auto *l = new QVBoxLayout(page);
    l->setSpacing(14);
    auto *select = new QHBoxLayout;
    devices = new QComboBox;
    devices->setMinimumWidth(360);
    select->addWidget(devices, 1);
    auto *refresh = new QPushButton("Refresh devices");
    select->addWidget(refresh);
    connect(refresh, &QPushButton::clicked, this, &Window::refreshDevices);
    l->addLayout(select);
    connect(devices, QOverload<int>::of(&QComboBox::currentIndexChanged), this, [this]() {
        if (audio.wanted()) {
            finishRecord();
            audio.start(devices->currentData().toString());
        }
    });
    auto *state = new QHBoxLayout;
    audioStatus = new QLabel("Audio server: checking…");
    obsStatus = new QLabel("OBS: waiting for capture");
    state->addWidget(audioStatus);
    state->addStretch();
    state->addWidget(obsStatus);
    l->addLayout(state);
    waveform = new Waveform(&audio);
    l->addWidget(waveform, 1);
    auto *center = new QHBoxLayout;
    startButton = new QPushButton("START");
    startButton->setObjectName("start");
    startButton->setMinimumSize(220, 70);
    center->addStretch();
    center->addWidget(startButton);
    center->addStretch();
    l->addLayout(center);
    statusLabel = new QLabel("STOPPED");
    statusLabel->setAlignment(Qt::AlignCenter);
    l->addWidget(statusLabel);
    connect(startButton, &QPushButton::clicked, this, [this]() {
        if (audio.wanted()) {
            finishRecord();
            audio.stop();
            startButton->setText("START");
        } else {
            audio.start(devices->currentData().toString());
            startButton->setText("STOP");
        }
    });
    auto *levels = new QGridLayout;
    inMeter = new QProgressBar;
    outMeter = new QProgressBar;
    for (auto *m : {inMeter, outMeter}) {
        m->setRange(0, 600);
        m->setTextVisible(false);
        m->setMaximumHeight(9);
    }
    levels->addWidget(new QLabel("INPUT"), 0, 0);
    levels->addWidget(inMeter, 0, 1);
    levels->addWidget(new QLabel("OUTPUT"), 1, 0);
    levels->addWidget(outMeter, 1, 1);
    l->addLayout(levels);
    meterText = new QLabel("15 ms algorithmic delay · server timing available when running");
    meterText->setObjectName("muted");
    meterText->setWordWrap(true);
    l->addWidget(meterText);
    auto *options = new QHBoxLayout;
    presets = new QComboBox;
    presets->addItems({"Podcast Studio", "YouTube Narrator", "Gaming Streamer", "Deep Warm Voice",
                       "Clean Interview", "Custom"});
    options->addWidget(presets, 1);
    connect(presets, QOverload<int>::of(&QComboBox::activated), this, &Window::loadPreset);
    auto *autoButton = new QPushButton("AUTO STUDIO VOICE");
    connect(autoButton, &QPushButton::clicked, this, &Window::analyze);
    options->addWidget(autoButton);
    rawMode = new QCheckBox("Raw A/B (safety limited)");
    connect(rawMode, &QCheckBox::toggled, this, [this]() { apply(); });
    options->addWidget(rawMode);
    l->addLayout(options);
    analysisStatus = new QLabel("Auto mode measures five seconds locally, after permission.");
    analysisStatus->setWordWrap(true);
    analysisStatus->setObjectName("muted");
    l->addWidget(analysisStatus);
    return page;
}
QWidget *Window::makeMixer() {
    auto *page = new QWidget;
    auto *outer = new QVBoxLayout(page);
    auto *scroll = new QScrollArea;
    scroll->setWidgetResizable(true);
    auto *content = new QWidget;
    auto *grid = new QGridLayout(content);
    struct Control {
        const char *key;
        const char *label;
        double min, max, value, step;
        const char *unit;
    };
    Control controls[] = {{"input", "Input gain", .25, 4, 1, .05, " ×"},
                          {"output", "Output gain", .1, 2, 1, .05, " ×"},
                          {"mix", "Noise suppression mix", 0, 1, .8, .05, ""},
                          {"hp", "High-pass cutoff", 40, 180, 80, 5, " Hz"},
                          {"warmth", "Warmth · 140 Hz", -6, 4, 0, .5, " dB"},
                          {"mud", "Mud · 300 Hz", -9, 3, -2, .5, " dB"},
                          {"presence", "Presence · 3 kHz", -4, 5, 1.5, .5, " dB"},
                          {"brightness", "Brightness · 9 kHz", -6, 4, 0, .5, " dB"},
                          {"gate", "Expander threshold", -80, -25, -55, 1, " dBFS"},
                          {"deess", "De-esser strength", 0, 1, .35, .05, ""},
                          {"threshold", "Compressor threshold", -40, -6, -18, 1, " dBFS"},
                          {"ratio", "Compressor ratio", 1, 8, 3, .25, " :1"},
                          {"attack", "Compressor attack", 1, 50, 10, 1, " ms"},
                          {"release", "Compressor release", 20, 500, 150, 10, " ms"},
                          {"knee", "Compressor knee", 1, 12, 6, 1, " dB"},
                          {"makeup", "Makeup gain", 0, 12, 2, .5, " dB"},
                          {"saturation", "Optional warmth drive", 0, .5, 0, .025, ""},
                          {"ceiling", "Limiter ceiling", -12, -1, -3, .5, " dBFS"}};
    for (unsigned i = 0; i < sizeof(controls) / sizeof(*controls); i++) {
        auto &c = controls[i];
        int row = int(i) / 2, col = (i % 2) * 2;
        grid->addWidget(new QLabel(c.label), row, col);
        grid->addWidget(knob(c.key, c.min, c.max, c.value, c.step, c.unit), row, col + 1);
    }
    scroll->setWidget(content);
    outer->addWidget(scroll);
    auto *help = new QLabel(
        "The 4× interpolated peak detector drives a 5 ms lookahead limiter. Ceiling and "
        "intersample behavior are tested; the meter is an estimate, not a certified BS.1770 "
        "loudness meter. Changes are smoothed. Use headphones to avoid feedback.");
    help->setWordWrap(true);
    outer->addWidget(help);
    return page;
}
QWidget *Window::makeSettings() {
    auto *page = new QWidget;
    auto *l = new QVBoxLayout(page);
    l->addWidget(title("AUDIO & RECOVERY"));
    lowCpu = new QCheckBox("Low CPU compatibility mode (bypasses RNNoise; retains tone/dynamics)");
    agc =
        new QCheckBox("Conservative automatic level assist (maximum +6 dB; pauses below -45 dBFS)");
    l->addWidget(lowCpu);
    l->addWidget(agc);
    connect(lowCpu, &QCheckBox::toggled, this, [this]() { apply(); });
    connect(agc, &QCheckBox::toggled, this, [this]() { apply(); });
    auto *repair = new QPushButton("Repair Audio Connection");
    connect(repair, &QPushButton::clicked, this, [this]() {
        finishRecord();
        audio.repair();
    });
    l->addWidget(repair);
    auto *text = new QLabel(
        "PulseAudio first · PipeWire uses pipewire-pulse (desktop validation pending).\n\nAudio "
        "remains on this machine. Recording is opt-in and uses temporary local files. No OBS "
        "settings are modified. The system default microphone is never changed.\n\nReconnect "
        "attempts run every two seconds for the exact selected source name. If the device name "
        "changes, use Refresh devices and select it again. Only this instance’s UUID-tagged "
        "modules are unloaded.\n\nTypical algorithmic delay: 15 ms. The Studio panel reports "
        "audio-server buffer timing separately; this is not a measured hardware round "
        "trip.\n\nNative application: Qt 5 / C++17 with bundled RNNoise v0.1. Physical microphone, "
        "J1800 performance and real-room voice quality require local verification.");
    text->setWordWrap(true);
    l->addWidget(text);
    l->addStretch();
    return page;
}
QWidget *Window::makePresets() {
    auto *page = new QWidget;
    auto *l = new QVBoxLayout(page);
    l->addWidget(title("YOUR VOICE, CONSERVATIVE STARTING POINTS"));
    auto *text = new QLabel(
        "Podcast Studio — balanced spoken voice, moderate noise control.\n\nYouTube Narrator — a "
        "little more presence and brightness.\n\nGaming Streamer — low CPU, neural suppression "
        "disabled.\n\nDeep Warm Voice — gentle 140 Hz warmth; no pitch shifting.\n\nClean "
        "Interview — lighter compression, no makeup gain.\n\nCustom — adjust the Mixer controls. "
        "Last-used settings persist locally.\n\nRaw A/B keeps aligned delay and the safety "
        "limiter. Disable other suppressors initially to avoid stacking. No preset removes all "
        "room reflections, echo, or other voices.");
    text->setWordWrap(true);
    l->addWidget(text);
    auto *save = new QPushButton("Save current controls locally");
    connect(save, &QPushButton::clicked, this, [this]() {
        for (auto it = knobs.begin(); it != knobs.end(); ++it)
            preferences.setValue("controls/" + it.key(), it.value()->value());
        preferences.sync();
        statusLabel->setText("Custom controls saved locally.");
    });
    l->addWidget(save);
    l->addStretch();
    return page;
}
QWidget *Window::makeRecording() {
    auto *page = new QWidget;
    auto *l = new QVBoxLayout(page);
    l->addWidget(title("RECORDING WORKSPACE"));
    auto *notice = new QLabel(
        "Record only when you choose. Raw and processed audio are kept in temporary local files "
        "for A/B preview. Stop before editing. Range edits are non-destructive; Undo/Redo restores "
        "them. Audio is discarded when the application closes unless exported.");
    notice->setWordWrap(true);
    l->addWidget(notice);
    auto *transport = new QHBoxLayout;
    recordButton = new QPushButton("● Record");
    pauseButton = new QPushButton("Pause");
    auto *stop = new QPushButton("Stop recording");
    transport->addWidget(recordButton);
    transport->addWidget(pauseButton);
    transport->addWidget(stop);
    l->addLayout(transport);
    connect(recordButton, &QPushButton::clicked, this, &Window::beginRecord);
    connect(stop, &QPushButton::clicked, this, &Window::finishRecord);
    connect(pauseButton, &QPushButton::clicked, this, [this]() {
        if (!recording)
            return;
        paused = !paused;
        if (audio.worker)
            audio.worker->recording = !paused;
        pauseButton->setText(paused ? "Resume" : "Pause");
    });
    timeline = new RecordingTimeline;
    l->addWidget(timeline);
    connect(timeline, &RecordingTimeline::selected, this,
            [this](double t) { splitPoint->setValue(t); });
    auto *g = new QGridLayout;
    trimStart = new QDoubleSpinBox;
    trimEnd = new QDoubleSpinBox;
    splitPoint = new QDoubleSpinBox;
    for (auto *spin : {trimStart, trimEnd, splitPoint}) {
        spin->setRange(0, 3600);
        spin->setDecimals(3);
        spin->setSuffix(" s");
    }
    g->addWidget(new QLabel("Keep range start"), 0, 0);
    g->addWidget(trimStart, 0, 1);
    g->addWidget(new QLabel("Keep range end"), 1, 0);
    g->addWidget(trimEnd, 1, 1);
    g->addWidget(new QLabel("Split point"), 2, 0);
    g->addWidget(splitPoint, 2, 1);
    auto *trim = new QPushButton("Trim to range");
    auto *split = new QPushButton("Split at cursor");
    auto *undo = new QPushButton("Undo");
    auto *redo = new QPushButton("Redo");
    g->addWidget(trim, 0, 2);
    g->addWidget(split, 2, 2);
    g->addWidget(undo, 3, 1);
    g->addWidget(redo, 3, 2);
    l->addLayout(g);
    connect(trim, &QPushButton::clicked, this, [this]() { edit(false); });
    connect(split, &QPushButton::clicked, this, [this]() { edit(true); });
    connect(undo, &QPushButton::clicked, this, [this]() {
        if (!recording && !undoStack.isEmpty()) {
            redoStack.push_back(segments);
            segments = undoStack.takeLast();
            refreshTimeline();
            recordStatus->setText("Edit undone.");
        }
    });
    connect(redo, &QPushButton::clicked, this, [this]() {
        if (!recording && !redoStack.isEmpty()) {
            undoStack.push_back(segments);
            segments = redoStack.takeLast();
            refreshTimeline();
            recordStatus->setText("Edit restored.");
        }
    });
    auto *options = new QHBoxLayout;
    previewMode = new QComboBox;
    previewMode->addItems({"Processed", "Raw"});
    exportFormat = new QComboBox;
    exportFormat->addItems({"WAV", "FLAC", "MP3"});
    exportChannels = new QComboBox;
    exportChannels->addItems({"Mono", "Stereo (dual mono)"});
    options->addWidget(previewMode);
    options->addWidget(exportFormat);
    options->addWidget(exportChannels);
    l->addLayout(options);
    auto *actions = new QHBoxLayout;
    auto *preview = new QPushButton("Preview selection (headphones)");
    auto *exportButton = new QPushButton("Export…");
    actions->addWidget(preview);
    actions->addWidget(exportButton);
    l->addLayout(actions);
    connect(preview, &QPushButton::clicked, this, [this]() { exportAudio(true); });
    connect(exportButton, &QPushButton::clicked, this, [this]() { exportAudio(false); });
    recordStatus = new QLabel("No recording. Start the Studio audio engine, then click Record.");
    recordStatus->setWordWrap(true);
    l->addWidget(recordStatus);
    l->addStretch();
    return page;
}
vc::Settings Window::settings() const {
    vc::Settings s;
    auto get = [&](const char *k) { return float(knobs[k]->value()); };
    s.input = get("input");
    s.output = get("output");
    s.mix = get("mix");
    s.highpass = get("hp");
    s.warmth = get("warmth");
    s.mud = get("mud");
    s.presence = get("presence");
    s.brightness = get("brightness");
    s.gate = get("gate");
    s.deess = get("deess");
    s.threshold = get("threshold");
    s.ratio = get("ratio");
    s.attack = get("attack");
    s.release = get("release");
    s.knee = get("knee");
    s.makeup = get("makeup");
    s.saturation = get("saturation");
    s.ceiling = get("ceiling");
    s.lowCpu = lowCpu->isChecked();
    s.agc = agc->isChecked();
    s.bypass = rawMode->isChecked();
    return s;
}
void Window::apply() {
    if (knobs.size() != 18 || !lowCpu || !agc || !rawMode)
        return;
    audio.setSettings(settings());
}
void Window::loadPreset(int index) {
    if (index == 5)
        return;
    vc::Settings s = vc::preset(index);
    QMap<QString, float> values = {
        {"input", s.input},       {"output", s.output},         {"mix", s.mix},
        {"hp", s.highpass},       {"warmth", s.warmth},         {"mud", s.mud},
        {"presence", s.presence}, {"brightness", s.brightness}, {"gate", s.gate},
        {"deess", s.deess},       {"threshold", s.threshold},   {"ratio", s.ratio},
        {"attack", s.attack},     {"release", s.release},       {"knee", s.knee},
        {"makeup", s.makeup},     {"saturation", s.saturation}, {"ceiling", s.ceiling}};
    for (auto it = values.begin(); it != values.end(); ++it)
        knobs[it.key()]->setValue(it.value());
    lowCpu->setChecked(s.lowCpu);
    presets->setCurrentIndex(index);
    apply();
}
void Window::refreshDevices() {
    QString selected = devices->currentData().toString();
    if (selected.isEmpty())
        selected = preferences.value("source").toString();
    devices->blockSignals(true);
    devices->clear();
    try {
        for (auto &d : microphones())
            devices->addItem(d.description, d.name);
        int index = devices->findData(selected);
        if (index < 0) {
            QString info = pulseCommand({"info"});
            for (QString line : info.split('\n'))
                if (line.startsWith("Default Source: "))
                    index = devices->findData(line.mid(16));
        }
        if (index >= 0)
            devices->setCurrentIndex(index);
        audioStatus->setText("Audio server: connected");
    } catch (const std::exception &e) {
        audioStatus->setText(QString::fromUtf8(e.what()));
    }
    devices->blockSignals(false);
}
void Window::update() {
    waveform->update();
    auto *w = audio.worker;
    if (!w)
        return;
    inMeter->setValue(std::clamp(int((dbfs(w->inputLevel) + 60) * 10), 0, 600));
    outMeter->setValue(std::clamp(int((dbfs(w->outputLevel) + 60) * 10), 0, 600));
    meterText->setText(QString("Input %1 dBFS · Output %2 dBFS · Reduction %3 dB · DSP %4% of one "
                               "core\n15 ms algorithmic + %5 ms server-reported buffers (−1 = "
                               "unavailable) · Underruns %6 · Input clipping frames %7")
                           .arg(dbfs(w->inputLevel), 0, 'f', 1)
                           .arg(dbfs(w->outputLevel), 0, 'f', 1)
                           .arg(w->reduction.load(), 0, 'f', 1)
                           .arg(w->dspLoad.load(), 0, 'f', 1)
                           .arg(w->serverLatencyMs.load(), 0, 'f', 1)
                           .arg(w->underruns.load())
                           .arg(w->clipping.load()));
    if (recording || analyzing) {
        QVector<float> raw, wet;
        w->capture(raw, wet);
        if (recording) {
            if (rawFile.write(reinterpret_cast<const char *>(raw.constData()), raw.size() * 4) !=
                    raw.size() * 4 ||
                wetFile.write(reinterpret_cast<const char *>(wet.constData()), wet.size() * 4) !=
                    wet.size() * 4) {
                finishRecord();
                recordStatus->setText("Recording stopped: disk write failed.");
                return;
            }
            recordedSamples += raw.size();
            recordStatus->setText(QString("%1 · %2 seconds · %3 MB on disk")
                                      .arg(paused ? "PAUSED" : "RECORDING")
                                      .arg(recordedSamples / 48000., 0, 'f', 1)
                                      .arg(recordedSamples * 8. / 1e6, 0, 'f', 1));
            if (recordedSamples >= qint64(48000) * 3600 || w->captureOverrun) {
                finishRecord();
                recordStatus->setText("Recording stopped: duration limit or capture overrun. "
                                      "Export the captured audio.");
            }
        }
        if (analyzing) {
            double blockPower = 0;
            for (float x : raw) {
                sumPower += x * x;
                blockPower += x * x;
                analysisPeak = std::max(analysisPeak, double(std::abs(x)));
                lastLow += .015 * (x - lastLow);
                lowPower += lastLow * lastLow;
                highPower += (x - lastLow) * (x - lastLow);
                analysisSamples++;
            }
            if (!raw.isEmpty())
                minRms = std::min(minRms, std::sqrt(blockPower / raw.size()));
            analysisFrames += raw.size();
            if (analysisFrames >= 48000 * 5) {
                analyzing = false;
                w->recording = false;
                float noiseDb = dbfs(minRms);
                knobs["gate"]->setValue(std::clamp(noiseDb + 4.f, -70.f, -40.f));
                if (lowPower > sumPower * .35)
                    knobs["hp"]->setValue(100);
                if (analysisPeak > .95)
                    knobs["input"]->setValue(.75);
                analysisStatus->setText(
                    QString("Local sample: quietest frame %1 dBFS · RMS %2 dBFS · peak %3 dBFS. "
                            "Conservative gate/high-pass adjusted. Lower hardware gain if "
                            "clipping. This is a signal estimate, not a speech-quality score.")
                        .arg(noiseDb, 0, 'f', 1)
                        .arg(dbfs(std::sqrt(sumPower / std::max<uint64_t>(1, analysisSamples))), 0,
                             'f', 1)
                        .arg(dbfs(analysisPeak), 0, 'f', 1));
            }
        }
    }
    static int tick = 0;
    if (++tick % 100 == 0) {
        try {
            QString sourceId;
            for (QString row : pulseCommand({"list", "short", "sources"}).split('\n'))
                if (row.section('\t', 1, 1) == audio.virtualSource())
                    sourceId = row.section('\t', 0, 0);
            bool found = false;
            for (QString block : pulseCommand({"list", "source-outputs"}).split("Source Output #"))
                if (block.contains("application.process.binary = \"obs\"") ||
                    block.contains("application.name = \"OBS")) {
                    for (QString line : block.split('\n'))
                        if (line.trimmed() == "Source: " + sourceId && !sourceId.isEmpty())
                            found = true;
                }
            obsStatus->setText(found ? "OBS: capture connected"
                                     : "OBS: select the virtual microphone");
        } catch (const std::exception &) {
            obsStatus->setText("OBS: audio server disconnected");
        }
    }
}
void Window::beginRecord() {
    if (recording)
        return;
    if (analyzing || !audio.worker || !audio.worker->ready) {
        recordStatus->setText("Start Studio audio first; finish analysis before recording.");
        return;
    }
    if (recordedSamples &&
        QMessageBox::question(this, "New recording",
                              "Discard the current unexported take and start a new recording?") !=
            QMessageBox::Yes)
        return;
    if (!rawFile.isOpen() && !rawFile.open())
        return;
    if (!wetFile.isOpen() && !wetFile.open())
        return;
    rawFile.resize(0);
    wetFile.resize(0);
    rawFile.seek(0);
    wetFile.seek(0);
    recordedSamples = 0;
    segments.clear();
    undoStack.clear();
    redoStack.clear();
    QVector<float> a, b;
    audio.worker->capture(a, b);
    audio.worker->captureOverrun = false;
    recording = true;
    paused = false;
    audio.worker->recording = true;
    pauseButton->setText("Pause");
}
void Window::finishRecord() {
    if (!recording)
        return;
    if (audio.worker) {
        audio.worker->recording = false;
        QVector<float> a, b;
        audio.worker->capture(a, b);
        rawFile.write(reinterpret_cast<const char *>(a.constData()), a.size() * 4);
        wetFile.write(reinterpret_cast<const char *>(b.constData()), b.size() * 4);
        recordedSamples += a.size();
    }
    recording = false;
    paused = false;
    pauseButton->setText("Pause");
    rawFile.flush();
    wetFile.flush();
    segments = {{0, recordedSamples}};
    refreshTimeline();
    trimStart->setValue(0);
    trimEnd->setValue(recordedSamples / 48000.);
    recordStatus->setText(QString("Stopped · %1 seconds. Ready to edit/export.")
                              .arg(recordedSamples / 48000., 0, 'f', 2));
}
void Window::edit(bool split) {
    if (recording || recordedSamples == 0)
        return;
    QVector<Segment> result;
    const qint64 first = qRound64(trimStart->value() * 48000),
                 last = qRound64(trimEnd->value() * 48000),
                 point = qRound64(splitPoint->value() * 48000);
    qint64 offset = 0;
    for (auto s : segments) {
        qint64 length = s.last - s.first;
        if (split) {
            if (point > offset && point < offset + length) {
                result.push_back({s.first, s.first + point - offset});
                result.push_back({s.first + point - offset, s.last});
            } else
                result.push_back(s);
        } else {
            qint64 a = std::max<qint64>(0, first - offset), b = std::min(length, last - offset);
            if (a < b)
                result.push_back({s.first + a, s.first + b});
        }
        offset += length;
    }
    if (result.isEmpty()) {
        recordStatus->setText("Choose a nonempty range within the edited timeline.");
        return;
    }
    undoStack.push_back(segments);
    if (undoStack.size() > 100)
        undoStack.removeFirst();
    redoStack.clear();
    segments = result;
    refreshTimeline();
    trimStart->setValue(0);
    qint64 n = 0;
    for (auto s : segments)
        n += s.last - s.first;
    trimEnd->setValue(n / 48000.);
    recordStatus->setText(QString("%1 segments · %2 seconds. Edits are undoable.")
                              .arg(segments.size())
                              .arg(n / 48000., 0, 'f', 2));
}
void Window::exportAudio(bool preview, const QString &path) {
    if (recording || segments.isEmpty()) {
        recordStatus->setText("Stop and select a recording first.");
        return;
    }
    if (exportProcess.state() != QProcess::NotRunning ||
        previewProcess.state() != QProcess::NotRunning)
        return;
    QString destination;
    if (!preview) {
        QString ext = exportFormat->currentText().toLower();
        destination = path.isEmpty()
                          ? QFileDialog::getSaveFileName(this, "Export edited recording",
                                                         QDir::homePath() + "/voicecraft." + ext,
                                                         "Audio (*." + ext + ")")
                          : path;
        if (destination.isEmpty())
            return;
    }
    if (!exportData.isOpen() && !exportData.open())
        return;
    exportData.resize(0);
    exportData.seek(0);
    QFile &file = previewMode->currentIndex() == 0 ? static_cast<QFile &>(wetFile)
                                                   : static_cast<QFile &>(rawFile);
    for (auto s : segments) {
        file.seek(s.first * 4);
        qint64 remaining = (s.last - s.first) * 4;
        while (remaining > 0) {
            QByteArray chunk = file.read(std::min<qint64>(remaining, 262144));
            if (chunk.isEmpty() || exportData.write(chunk) != chunk.size()) {
                recordStatus->setText("Cannot prepare export: disk I/O error.");
                return;
            }
            remaining -= chunk.size();
        }
    }
    exportData.flush();
    if (preview) {
        previewProcess.start("ffplay", {"-nodisp", "-autoexit", "-loglevel", "error", "-f", "f32le",
                                        "-ar", "48000", "-ac", "1", exportData.fileName()});
        recordStatus->setText("Preview playing through the default output. Use headphones.");
        return;
    }
    QStringList args = {"-nostdin", "-v",
                        "error",    "-y",
                        "-f",       "f32le",
                        "-ar",      "48000",
                        "-ac",      "1",
                        "-i",       exportData.fileName(),
                        "-ac",      exportChannels->currentIndex() == 0 ? "1" : "2"};
    if (exportFormat->currentIndex() == 0)
        args << "-c:a"
             << "pcm_s24le";
    else if (exportFormat->currentIndex() == 1)
        args << "-c:a"
             << "flac";
    else
        args << "-c:a"
             << "libmp3lame"
             << "-b:a"
             << "192k";
    args << destination;
    exportProcess.start("ffmpeg", args);
    recordStatus->setText("Exporting…");
}
void Window::analyze() {
    if (analyzing || recording || !audio.worker || !audio.worker->ready) {
        analysisStatus->setText("Start Studio first. Stop recording before analysis.");
        return;
    }
    if (QMessageBox::question(
            this, "Local microphone analysis",
            "Analyze five seconds of microphone audio locally? Speak normally, then leave a short "
            "quiet pause. No audio is uploaded or saved by this analysis.") != QMessageBox::Yes)
        return;
    QVector<float> a, b;
    audio.worker->capture(a, b);
    analysisFrames = 0;
    analysisSamples = 0;
    minRms = 1;
    sumPower = lowPower = highPower = analysisPeak = lastLow = 0;
    analyzing = true;
    audio.worker->recording = true;
    analysisStatus->setText("Analyzing locally for five seconds… speak, then pause.");
}
void Window::closeEvent(QCloseEvent *e) {
    if (recording && QMessageBox::question(
                         this, "Recording active",
                         "Stop recording and close? Export first if you want to keep the take.") !=
                         QMessageBox::Yes) {
        e->ignore();
        return;
    }
    finishRecord();
    audio.stop();
    exportProcess.kill();
    previewProcess.kill();
    exportProcess.waitForFinished();
    previewProcess.waitForFinished();
    preferences.setValue("source", devices->currentData());
    preferences.setValue("preset", presets->currentIndex());
    preferences.setValue("lowCpu", lowCpu->isChecked());
    for (auto it = knobs.begin(); it != knobs.end(); ++it)
        preferences.setValue("controls/" + it.key(), it.value()->value());
    e->accept();
}

void Window::refreshTimeline() {
    QVector<QPair<qint64, qint64>> ranges;
    for (auto s : segments)
        ranges.push_back({s.first, s.last});
    timeline->load(wetFile.fileName(), ranges);
}
