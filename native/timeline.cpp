#include "timeline.h"
#include <QFile>
#include <QMouseEvent>
#include <QPainter>
#include <QWheelEvent>
#include <algorithm>
#include <cmath>
void RecordingTimeline::load(const QString &path, const QVector<QPair<qint64, qint64>> &segments) {
    qint64 samples = 0;
    for (auto s : segments)
        samples += s.second - s.first;
    seconds = samples / 48000.;
    peaks.fill(0, 1024);
    QFile f(path);
    if (!f.open(QIODevice::ReadOnly))
        return;
    for (int i = 0; i < 1024; i++) {
        qint64 position = samples * i / 1024, offset = 0;
        for (auto s : segments) {
            qint64 length = s.second - s.first;
            if (position < offset + length) {
                f.seek((s.first + position - offset) * 4);
                QByteArray chunk =
                    f.read(std::min<qint64>(1024, s.second - s.first - (position - offset)) * 4);
                const float *p = reinterpret_cast<const float *>(chunk.constData());
                for (int j = 0; j < chunk.size() / 4; j++)
                    peaks[i] = std::max(peaks[i], std::abs(p[j]));
                break;
            }
            offset += length;
        }
    }
    zoom = 1;
    offset = 0;
    cursor = 0;
    update();
}
void RecordingTimeline::paintEvent(QPaintEvent *) {
    QPainter p(this);
    p.fillRect(rect(), QColor("#111520"));
    double visible = seconds / zoom;
    p.setPen(QColor("#333c51"));
    p.drawLine(0, height() / 2, width(), height() / 2);
    p.setPen(QColor("#a8f0c6"));
    if (seconds > 0) {
        for (int x = 0; x < width(); x++) {
            double t = offset + x * visible / width();
            int i = std::clamp(int(t / seconds * 1024), 0, 1023);
            float a = peaks.value(i) * height() * .8;
            p.drawLine(x, height() / 2 - a, x, height() / 2 + a);
        }
        p.setPen(QColor("#f0e6cb"));
        int x = int((cursor - offset) / visible * width());
        p.drawLine(x, 0, x, height());
    }
    p.setPen(QColor("#8b95aa"));
    p.drawText(12, 20,
               QString("%1–%2 s   ·   %3× zoom   ·   cursor %4 s")
                   .arg(offset, 0, 'f', 2)
                   .arg(offset + visible, 0, 'f', 2)
                   .arg(zoom, 0, 'f', 1)
                   .arg(cursor, 0, 'f', 3));
}
void RecordingTimeline::mousePressEvent(QMouseEvent *event) {
    if (seconds <= 0)
        return;
    cursor = std::clamp(offset + event->x() * seconds / (zoom * width()), 0., seconds);
    emit selected(cursor);
    update();
}
void RecordingTimeline::wheelEvent(QWheelEvent *event) {
    if (seconds <= 0)
        return;
    if (event->modifiers() & Qt::ShiftModifier)
        offset = std::clamp(offset + (event->angleDelta().y() > 0 ? -1 : 1) * seconds / (zoom * 8),
                            0., seconds - seconds / zoom);
    else {
        zoom = std::clamp(zoom * (event->angleDelta().y() > 0 ? 1.4 : 1 / 1.4), 1., 64.);
        offset = std::clamp(cursor - seconds / (zoom * 2), 0., seconds - seconds / zoom);
    }
    event->accept();
    update();
}
