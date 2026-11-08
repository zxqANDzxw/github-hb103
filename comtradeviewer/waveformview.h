#ifndef WAVEFORMVIEW_H
#define WAVEFORMVIEW_H

#include <QWidget>
#include <QVector>

#include "comtrade.h"

// 自绘波形视图：模拟量按通道分带显示，开关量以高低电平带显示，
// 支持滚轮缩放时间轴、按住左键拖动平移、双击复位
class WaveformView : public QWidget
{
    Q_OBJECT
public:
    explicit WaveformView(QWidget *parent = nullptr);

    void setData(const CtData &data);
    void setAnalogVisible(int index, bool visible);
    void setDigitalVisible(int index, bool visible);
    void resetView();
    bool hasData() const { return m_data.sampleCount > 0; }

signals:
    void cursorInfo(const QString &text);

protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;

private:
    QRect plotRect() const;
    int mapX(double time) const;
    double timeAt(int x) const;
    int sampleIndexAt(double time) const;
    void drawAnalog(QPainter &painter, int analogIndex, const QRect &band,
                    const QColor &color, int firstSample, int lastSample);
    void drawDigital(QPainter &painter, int digitalIndex, const QRect &band,
                     int firstSample, int lastSample);
    QString buildCursorText(double time) const;

    CtData m_data;
    QVector<bool> m_analogVisible;
    QVector<bool> m_digitalVisible;
    QVector<double> m_dataMin;
    QVector<double> m_dataMax;

    double m_viewStart = 0.0;
    double m_viewEnd = 1.0;
    int m_cursorX = -1;
    bool m_dragging = false;
    int m_dragLastX = 0;
};

#endif // WAVEFORMVIEW_H
