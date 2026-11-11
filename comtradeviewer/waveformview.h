#ifndef WAVEFORMVIEW_H
#define WAVEFORMVIEW_H

#include <QWidget>
#include <QVector>

#include "comtrade.h"

class QScrollBar;
class QResizeEvent;

// 自绘波形视图：模拟量按通道分带显示，开关量以高低电平带显示。
// 通道纵向排列，通道数较多时右侧出现垂直滑块，鼠标滚轮上下滚动；
// Ctrl+滚轮缩放时间轴，按住左键拖动平移，双击复位
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
    // 左右键时标变化时，把 “t1 / t2 / t2-t1” 文本交给状态栏
    void selectionInfo(const QString &text);

protected:
    void paintEvent(QPaintEvent *event) override;
    void wheelEvent(QWheelEvent *event) override;
    void mousePressEvent(QMouseEvent *event) override;
    void mouseMoveEvent(QMouseEvent *event) override;
    void mouseReleaseEvent(QMouseEvent *event) override;
    void mouseDoubleClickEvent(QMouseEvent *event) override;
    void leaveEvent(QEvent *event) override;
    void resizeEvent(QResizeEvent *event) override;

private:
    QRect plotRect() const;
    int mapX(double time) const;
    double timeAt(int x) const;
    int sampleIndexAt(double time) const;
    void drawAnalog(QPainter &painter, int analogIndex, const QRect &band,
                    const QColor &color, int firstSample, int lastSample);
    void drawDigital(QPainter &painter, int digitalIndex, const QRect &band,
                     int firstSample, int lastSample);
    QString buildSelectionText() const;
    // 取指定采样点的模拟量相量（整工频周期 DFT），返回形如 “59.684V∠-20.818°”
    QString phasorText(int analogIndex, int sample) const;
    // 按当前可见通道计算纵向滚动范围，并摆好滑块位置
    void updateScrollRange();

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

    QScrollBar *m_vScroll = nullptr; // 通道纵向滑块
    int m_scrollY = 0;               // 通道区当前纵向偏移（像素）

    double m_t1 = -1.0;              // 左键时标（秒）
    double m_t2 = -1.0;              // 右键时标（秒）
};

#endif // WAVEFORMVIEW_H
