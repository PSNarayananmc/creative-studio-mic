#pragma once
#include <QPair>
#include <QVector>
#include <QWidget>
class RecordingTimeline : public QWidget {
    Q_OBJECT
    QVector<float> peaks;
    double seconds = 0, zoom = 1, offset = 0, cursor = 0;

  public:
    explicit RecordingTimeline(QWidget *parent = nullptr) : QWidget(parent) {
        setMinimumHeight(130);
        setToolTip(
            "Click to move the edit cursor. Mouse wheel zooms. Shift+wheel scrolls the timeline.");
    }
    void load(const QString &file, const QVector<QPair<qint64, qint64>> &segments);

  protected:
    void paintEvent(QPaintEvent *) override;
    void mousePressEvent(QMouseEvent *) override;
    void wheelEvent(QWheelEvent *) override;
  signals:
    void selected(double seconds);
};
