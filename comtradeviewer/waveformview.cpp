#include "waveformview.h"

#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QFontMetrics>
#include <QPolygonF>
#include <QStringList>
#include <QPair>
#include <QtGlobal>
#include <cmath>

namespace {

const int AXIS_HEIGHT = 24;   // 底部时间轴高度
const int DIGITAL_BAND_HEIGHT = 16;

QColor channelColor(int index)
{
    static const QColor colors[] = {
        QColor(31, 119, 180), QColor(214, 39, 40), QColor(44, 160, 44),
        QColor(148, 103, 189), QColor(255, 127, 14), QColor(23, 190, 207),
        QColor(140, 86, 75), QColor(227, 119, 194), QColor(188, 189, 34),
        QColor(90, 90, 96)
    };
    return colors[index % int(sizeof(colors) / sizeof(colors[0]))];
}

double niceStep(double rough)
{
    if (rough <= 0.0) {
        return 1.0;
    }
    const double exp10 = std::pow(10.0, std::floor(std::log10(rough)));
    const double fraction = rough / exp10;
    double multiplier = 1.0;
    if (fraction >= 5.0) {
        multiplier = 5.0;
    } else if (fraction >= 2.0) {
        multiplier = 2.0;
    }
    return multiplier * exp10;
}

} // namespace

WaveformView::WaveformView(QWidget *parent)
    : QWidget(parent)
{
    setMinimumSize(320, 200);
    setMouseTracking(true);
    setAutoFillBackground(true);
}

void WaveformView::setData(const CtData &data)
{
    m_data = data;
    m_analogVisible = QVector<bool>(data.analogs.size(), false);
    m_digitalVisible = QVector<bool>(data.digitals.size(), false);
    m_dataMin = QVector<double>(data.analogs.size(), -1.0);
    m_dataMax = QVector<double>(data.analogs.size(), 1.0);

    // 统计每个模拟量通道的工程值范围，供分带绘图
    for (int c = 0; c < data.analogs.size(); ++c) {
        const QVector<double> &values = data.analogs.at(c).values;
        if (values.isEmpty()) {
            continue;
        }
        double lo = values.first();
        double hi = lo;
        for (int i = 1; i < values.size(); ++i) {
            const double v = values.at(i);
            if (v < lo) {
                lo = v;
            }
            if (v > hi) {
                hi = v;
            }
        }
        if (hi - lo < 1e-9) {
            lo -= 1.0;
            hi += 1.0;
        }
        m_dataMin[c] = lo;
        m_dataMax[c] = hi;
    }

    resetView();
}

void WaveformView::setAnalogVisible(int index, bool visible)
{
    if (index < 0 || index >= m_analogVisible.size()) {
        return;
    }
    m_analogVisible[index] = visible;
    update();
}

void WaveformView::setDigitalVisible(int index, bool visible)
{
    if (index < 0 || index >= m_digitalVisible.size()) {
        return;
    }
    m_digitalVisible[index] = visible;
    update();
}

void WaveformView::resetView()
{
    m_viewStart = 0.0;
    m_viewEnd = m_data.duration > 0.0 ? m_data.duration : 1.0;
    update();
}

QRect WaveformView::plotRect() const
{
    return rect().adjusted(0, 0, -1, -AXIS_HEIGHT);
}

int WaveformView::mapX(double time) const
{
    const QRect plot = plotRect();
    if (m_viewEnd <= m_viewStart) {
        return plot.left();
    }
    const double ratio = (time - m_viewStart) / (m_viewEnd - m_viewStart);
    return plot.left() + int(ratio * plot.width());
}

double WaveformView::timeAt(int x) const
{
    const QRect plot = plotRect();
    if (plot.width() <= 0) {
        return m_viewStart;
    }
    const double ratio = double(x - plot.left()) / double(plot.width());
    return m_viewStart + ratio * (m_viewEnd - m_viewStart);
}

int WaveformView::sampleIndexAt(double time) const
{
    const QVector<double> &times = m_data.times;
    if (times.isEmpty()) {
        return 0;
    }
    if (time <= times.first()) {
        return 0;
    }
    if (time >= times.last()) {
        return times.size() - 1;
    }
    int lo = 0;
    int hi = times.size() - 1;
    while (lo < hi) {
        const int mid = (lo + hi) / 2;
        if (times.at(mid) <= time) {
            lo = mid + 1;
        } else {
            hi = mid;
        }
    }
    return qMax(0, lo - 1);
}

void WaveformView::paintEvent(QPaintEvent *)
{
    QPainter painter(this);
    painter.fillRect(rect(), QColor(255, 255, 255));
    const QRect plot = plotRect();

    if (!hasData() || plot.width() <= 0 || plot.height() <= 0) {
        painter.setPen(QColor(120, 120, 124));
        painter.drawText(rect(), Qt::AlignCenter, QStringLiteral("请打开 COMTRADE 波形文件"));
        return;
    }

    QVector<int> analogShown;
    QVector<int> digitalShown;
    for (int i = 0; i < m_analogVisible.size(); ++i) {
        if (m_analogVisible.at(i)) {
            analogShown.append(i);
        }
    }
    for (int i = 0; i < m_digitalVisible.size(); ++i) {
        if (m_digitalVisible.at(i)) {
            digitalShown.append(i);
        }
    }

    const QRect analogArea(plot.left(), plot.top(), plot.width(),
                           plot.height() - digitalShown.size() * DIGITAL_BAND_HEIGHT);
    const int bandHeight = analogShown.isEmpty() ? 0 : analogArea.height() / analogShown.size();

    // 通道分带背景
    for (int k = 0; k < analogShown.size(); ++k) {
        const QRect band(plot.left(), analogArea.top() + k * bandHeight,
                         plot.width(), bandHeight);
        painter.fillRect(band, (k % 2) ? QColor(244, 246, 250) : QColor(251, 252, 254));
    }
    for (int k = 0; k < digitalShown.size(); ++k) {
        const QRect band(plot.left(),
                         analogArea.bottom() + 1 + k * DIGITAL_BAND_HEIGHT,
                         plot.width(), DIGITAL_BAND_HEIGHT);
        painter.fillRect(band, (k % 2) ? QColor(240, 247, 241) : QColor(249, 252, 249));
    }

    // 时间网格
    const double span = m_viewEnd - m_viewStart;
    const double step = niceStep(span / 6.0);
    const double firstTick = std::ceil(m_viewStart / step) * step;
    QVector<QPair<int, double> > ticks;
    for (double t = firstTick; t <= m_viewEnd + 1e-12; t += step) {
        ticks.append(qMakePair(mapX(t), t));
    }
    painter.setPen(QPen(QColor(226, 228, 233), 1));
    for (int i = 0; i < ticks.size(); ++i) {
        painter.drawLine(ticks.at(i).first, plot.top(), ticks.at(i).first, plot.bottom());
    }

    const int firstSample = sampleIndexAt(m_viewStart);
    const int lastSample = qMax(firstSample, sampleIndexAt(m_viewEnd));

    // 模拟量曲线
    for (int k = 0; k < analogShown.size(); ++k) {
        const int channel = analogShown.at(k);
        const QRect band(plot.left(), analogArea.top() + k * bandHeight,
                         plot.width(), bandHeight);
        drawAnalog(painter, channel, band, channelColor(channel), firstSample, lastSample);

        const CtAnalogChannel &info = m_data.analogs.at(channel);
        QString label = info.id;
        if (!info.unit.isEmpty()) {
            label += QStringLiteral(" [") + info.unit + QLatin1Char(']');
        }
        QFontMetrics fm(painter.font());
        const int textWidth = fm.width(label);
        painter.fillRect(QRect(band.left() + 2, band.top() + 1, textWidth + 6, fm.height() + 2),
                         QColor(255, 255, 255, 215));
        painter.setPen(QColor(55, 55, 60));
        painter.drawText(band.left() + 5, band.top() + 1 + fm.ascent() + 1, label);

        if (bandHeight >= 28) {
            painter.setPen(QColor(150, 152, 158));
            painter.drawText(QRect(band.right() - 92, band.top() + 1, 90, fm.height()),
                             Qt::AlignRight, QString::number(m_dataMax.at(channel), 'g', 4));
            painter.drawText(QRect(band.right() - 92, band.bottom() - fm.height() - 1, 90, fm.height()),
                             Qt::AlignRight, QString::number(m_dataMin.at(channel), 'g', 4));
        }
    }

    // 开关量
    for (int k = 0; k < digitalShown.size(); ++k) {
        const int channel = digitalShown.at(k);
        const QRect band(plot.left(),
                         analogArea.bottom() + 1 + k * DIGITAL_BAND_HEIGHT,
                         plot.width(), DIGITAL_BAND_HEIGHT);
        drawDigital(painter, channel, band, firstSample, lastSample);

        const QString label = m_data.digitals.at(channel).id;
        QFontMetrics fm(painter.font());
        const int textWidth = fm.width(label);
        painter.fillRect(QRect(band.left() + 2, band.top(), textWidth + 6, band.height()),
                         QColor(255, 255, 255, 210));
        painter.setPen(QColor(30, 110, 50));
        painter.drawText(band.left() + 5, band.top() + fm.ascent() + 1, label);
    }

    if (analogShown.isEmpty() && digitalShown.isEmpty()) {
        painter.setPen(QColor(120, 120, 124));
        painter.drawText(plot, Qt::AlignCenter, QStringLiteral("请在左侧勾选要显示的通道"));
    }

    // 时间轴标签
    painter.setPen(QColor(200, 202, 208));
    painter.drawLine(plot.left(), plot.bottom() + 1, plot.right(), plot.bottom() + 1);
    painter.setPen(QColor(90, 92, 98));
    int decimals = 3;
    if (step >= 1.0) {
        decimals = 1;
    } else if (step >= 0.1) {
        decimals = 2;
    } else if (step >= 0.01) {
        decimals = 3;
    } else if (step >= 0.001) {
        decimals = 4;
    } else {
        decimals = 6;
    }
    for (int i = 0; i < ticks.size(); ++i) {
        const QString label = QString::number(ticks.at(i).second, 'f', decimals) + QStringLiteral(" s");
        painter.drawText(QRect(ticks.at(i).first - 50, plot.bottom() + 3, 100, AXIS_HEIGHT - 3),
                         Qt::AlignHCenter, label);
    }

    // 光标
    if (m_cursorX >= plot.left() && m_cursorX <= plot.right()) {
        painter.setPen(QPen(QColor(200, 70, 70), 1, Qt::DashLine));
        painter.drawLine(m_cursorX, plot.top(), m_cursorX, plot.bottom());
        painter.setPen(QColor(200, 70, 70));
        painter.drawText(m_cursorX + 3, plot.top() + 12,
                         QString::number(timeAt(m_cursorX), 'f', 6) + QStringLiteral(" s"));
    }
}

void WaveformView::drawAnalog(QPainter &painter, int analogIndex, const QRect &band,
                              const QColor &color, int firstSample, int lastSample)
{
    if (band.height() <= 6) {
        return;
    }
    const QVector<double> &values = m_data.analogs.at(analogIndex).values;
    if (values.isEmpty()) {
        return;
    }
    const double lo = m_dataMin.at(analogIndex);
    const double hi = m_dataMax.at(analogIndex);
    const int top = band.top() + 6;
    const int bottom = band.bottom() - 4;
    if (bottom <= top) {
        return;
    }
    auto yOf = [&](double value) -> int {
        const double ratio = (value - lo) / (hi - lo);
        return bottom - int(ratio * (bottom - top));
    };

    painter.setPen(QPen(color, 1));
    const int count = lastSample - firstSample + 1;
    if (count > band.width() * 2) {
        // 每像素列取最小/最大值，画出波形包络（缩小时依然流畅）
        for (int x = band.left(); x <= band.right(); ++x) {
            int s0 = sampleIndexAt(timeAt(x));
            int s1 = sampleIndexAt(timeAt(x + 1));
            if (s1 > s0) {
                --s1;
            }
            s0 = qBound(firstSample, s0, lastSample);
            s1 = qBound(firstSample, s1, lastSample);
            if (s1 < s0) {
                s1 = s0;
            }
            double mn = values.at(s0);
            double mx = mn;
            for (int s = s0; s <= s1; ++s) {
                const double v = values.at(s);
                if (v < mn) {
                    mn = v;
                }
                if (v > mx) {
                    mx = v;
                }
            }
            const int y0 = yOf(mx);
            const int y1 = yOf(mn);
            if (y0 == y1) {
                painter.drawPoint(x, y0);
            } else {
                painter.drawLine(x, y0, x, y1);
            }
        }
    } else {
        QPolygonF polyline;
        polyline.reserve(count);
        for (int s = firstSample; s <= lastSample; ++s) {
            polyline.append(QPointF(mapX(m_data.times.at(s)), yOf(values.at(s))));
        }
        painter.drawPolyline(polyline);
    }
}

void WaveformView::drawDigital(QPainter &painter, int digitalIndex, const QRect &band,
                               int firstSample, int lastSample)
{
    const QVector<quint8> &values = m_data.digitals.at(digitalIndex).values;
    if (values.isEmpty()) {
        return;
    }
    const int yHigh = band.top() + 3;
    const int yLow = band.bottom() - 2;
    painter.setPen(QPen(QColor(30, 140, 60), 1));

    int previousX = band.left();
    int previousY = -1;
    for (int x = band.left(); x <= band.right(); ++x) {
        int s = sampleIndexAt(timeAt(x));
        s = qBound(firstSample, s, lastSample);
        if (s >= values.size()) {
            continue;
        }
        const int y = values.at(s) ? yHigh : yLow;
        if (previousY >= 0) {
            painter.drawLine(previousX, previousY, x, previousY);
            if (y != previousY) {
                painter.drawLine(x, previousY, x, y);
            }
        }
        previousX = x;
        previousY = y;
    }
    if (previousY >= 0) {
        painter.drawLine(previousX, previousY, band.right(), previousY);
    }
}

QString WaveformView::buildCursorText(double time) const
{
    const int sample = sampleIndexAt(time);
    QStringList parts;
    parts << QStringLiteral("t = %1 s").arg(time, 0, 'f', 6);
    for (int i = 0; i < m_analogVisible.size(); ++i) {
        if (!m_analogVisible.at(i)) {
            continue;
        }
        const CtAnalogChannel &channel = m_data.analogs.at(i);
        if (sample < channel.values.size()) {
            parts << QStringLiteral("%1 = %2 %3")
                         .arg(channel.id)
                         .arg(channel.values.at(sample), 0, 'f', 3)
                         .arg(channel.unit);
        }
    }
    for (int i = 0; i < m_digitalVisible.size(); ++i) {
        if (!m_digitalVisible.at(i)) {
            continue;
        }
        const CtDigitalChannel &channel = m_data.digitals.at(i);
        if (sample < channel.values.size()) {
            parts << QStringLiteral("%1 = %2").arg(channel.id).arg(int(channel.values.at(sample)));
        }
    }
    return parts.join(QStringLiteral("   "));
}

void WaveformView::wheelEvent(QWheelEvent *event)
{
    if (!hasData()) {
        return;
    }
    const double fullSpan = m_data.duration > 0.0 ? m_data.duration : 1.0;
    const double span = m_viewEnd - m_viewStart;
    const double cursorTime = timeAt(event->pos().x());
    const double factor = (event->delta() > 0) ? 0.8 : 1.25;
    const double minSpan = qMax(fullSpan * 1e-5, 1e-6);
    double newSpan = qBound(minSpan, span * factor, fullSpan);
    if (qAbs(newSpan - span) < 1e-15) {
        return;
    }
    const double ratio = span > 0.0 ? (cursorTime - m_viewStart) / span : 0.0;
    double start = cursorTime - ratio * newSpan;
    double end = start + newSpan;
    if (start < 0.0) {
        end -= start;
        start = 0.0;
    }
    if (end > fullSpan) {
        start -= (end - fullSpan);
        end = fullSpan;
    }
    m_viewStart = qMax(0.0, start);
    m_viewEnd = end;
    update();
}

void WaveformView::mousePressEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton && hasData()) {
        m_dragging = true;
        m_dragLastX = event->pos().x();
        setCursor(Qt::ClosedHandCursor);
    }
}

void WaveformView::mouseMoveEvent(QMouseEvent *event)
{
    const QRect plot = plotRect();
    if (m_dragging && plot.width() > 0) {
        const double span = m_viewEnd - m_viewStart;
        const double delta = -double(event->pos().x() - m_dragLastX) * span / plot.width();
        const double fullSpan = m_data.duration > 0.0 ? m_data.duration : 1.0;
        double start = m_viewStart + delta;
        double end = m_viewEnd + delta;
        if (start < 0.0) {
            end -= start;
            start = 0.0;
        }
        if (end > fullSpan) {
            start -= (end - fullSpan);
            end = fullSpan;
        }
        m_viewStart = qMax(0.0, start);
        m_viewEnd = end;
        m_dragLastX = event->pos().x();
        update();
        return;
    }

    m_cursorX = event->pos().x();
    update();
    if (m_cursorX >= plot.left() && m_cursorX <= plot.right()) {
        emit cursorInfo(buildCursorText(timeAt(m_cursorX)));
    } else {
        emit cursorInfo(QString());
    }
}

void WaveformView::mouseReleaseEvent(QMouseEvent *event)
{
    if (event->button() == Qt::LeftButton) {
        m_dragging = false;
        setCursor(Qt::ArrowCursor);
    }
}

void WaveformView::mouseDoubleClickEvent(QMouseEvent *)
{
    resetView();
}

void WaveformView::leaveEvent(QEvent *)
{
    m_cursorX = -1;
    update();
    emit cursorInfo(QString());
}
