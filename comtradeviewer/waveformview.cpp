#include "waveformview.h"

#include <QPainter>
#include <QMouseEvent>
#include <QWheelEvent>
#include <QResizeEvent>
#include <QScrollBar>
#include <QFontMetrics>
#include <QPolygonF>
#include <QStringList>
#include <QPair>
#include <QtGlobal>
#include <cmath>

namespace {

const int AXIS_HEIGHT = 24;              // 底部时间轴高度
const int ANALOG_BAND_HEIGHT = 88;        // 单路模拟量基础带高
const int DIGITAL_BAND_HEIGHT = 20;       // 单路开关量基础带高
const int SCROLL_STEP = 80;               // 滚轮一格滚动的像素

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
    setFocusPolicy(Qt::StrongFocus);

    m_vScroll = new QScrollBar(Qt::Vertical, this);
    m_vScroll->setSingleStep(SCROLL_STEP / 4);
    m_vScroll->setPageStep(200);
    m_vScroll->hide();
    connect(m_vScroll, &QScrollBar::valueChanged, this, [this](int value){
        m_scrollY = value;
        update();
    });
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

    m_scrollY = 0;
    m_t1 = -1.0;
    m_t2 = -1.0;
    resetView();
    updateScrollRange();
}

void WaveformView::setAnalogVisible(int index, bool visible)
{
    if (index < 0 || index >= m_analogVisible.size()) {
        return;
    }
    m_analogVisible[index] = visible;
    updateScrollRange();
    update();
}

void WaveformView::setDigitalVisible(int index, bool visible)
{
    if (index < 0 || index >= m_digitalVisible.size()) {
        return;
    }
    m_digitalVisible[index] = visible;
    updateScrollRange();
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
    const int sbWidth = m_vScroll->isVisible() ? m_vScroll->width() : 0;
    return QRect(0, 0, width() - sbWidth - 1, height() - AXIS_HEIGHT);
}

// 计算通道区总高度；通道较少时按比例拉伸铺满视口
static int contentHeightOf(int analogCount, int digitalCount, int available,
                           int *analogBand, int *digitalBand)
{
    int bandA = ANALOG_BAND_HEIGHT;
    int bandD = DIGITAL_BAND_HEIGHT;
    int total = analogCount * bandA + digitalCount * bandD;
    if (total > 0 && total < available) {
        const double scale = double(available) / double(total);
        bandA = int(bandA * scale);
        bandD = int(bandD * scale);
        total = analogCount * bandA + digitalCount * bandD;
    }
    *analogBand = bandA;
    *digitalBand = bandD;
    return total;
}

void WaveformView::updateScrollRange()
{
    int analogCount = 0;
    int digitalCount = 0;
    for (int i = 0; i < m_analogVisible.size(); ++i) {
        if (m_analogVisible.at(i)) {
            ++analogCount;
        }
    }
    for (int i = 0; i < m_digitalVisible.size(); ++i) {
        if (m_digitalVisible.at(i)) {
            ++digitalCount;
        }
    }

    const int available = height() - AXIS_HEIGHT;
    int bandA = 0;
    int bandD = 0;
    const int content = contentHeightOf(analogCount, digitalCount, available, &bandA, &bandD);
    const int overflow = qMax(0, content - available);

    m_vScroll->setVisible(overflow > 0);
    m_vScroll->setRange(0, overflow);
    m_vScroll->setPageStep(qMax(1, available));
    m_vScroll->setSingleStep(qMax(1, content / 20));

    const int sbWidth = m_vScroll->sizeHint().width();
    m_vScroll->setGeometry(width() - sbWidth, 0, sbWidth, qMax(0, available));

    if (m_scrollY > overflow) {
        m_scrollY = overflow;
    }
    if (m_vScroll->value() != m_scrollY) {
        m_vScroll->setValue(m_scrollY);
    }
}

void WaveformView::resizeEvent(QResizeEvent *event)
{
    QWidget::resizeEvent(event);
    updateScrollRange();
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

    int bandA = ANALOG_BAND_HEIGHT;
    int bandD = DIGITAL_BAND_HEIGHT;
    contentHeightOf(analogShown.size(), digitalShown.size(), plot.height(), &bandA, &bandD);

    // 通道区允许上下滚动，绘制时裁掉时间轴区域
    painter.save();
    painter.setClipRect(plot);

    const int analogTop = plot.top() - m_scrollY;
    for (int k = 0; k < analogShown.size(); ++k) {
        const QRect band(plot.left(), analogTop + k * bandA, plot.width(), bandA);
        if (band.bottom() < plot.top() || band.top() > plot.bottom()) {
            continue;
        }
        painter.fillRect(band, (k % 2) ? QColor(244, 246, 250) : QColor(251, 252, 254));
    }
    for (int k = 0; k < digitalShown.size(); ++k) {
        const int top = analogTop + analogShown.size() * bandA + k * bandD;
        const QRect band(plot.left(), top, plot.width(), bandD);
        if (band.bottom() < plot.top() || band.top() > plot.bottom()) {
            continue;
        }
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

    // 相量参考点：优先取鼠标光标处的采样点，否则取触发时刻
    int refSample = 0;
    if (m_cursorX >= plot.left() && m_cursorX <= plot.right()) {
        refSample = sampleIndexAt(timeAt(m_cursorX));
    } else if (m_data.startTime.isValid() && m_data.triggerTime.isValid()) {
        refSample = sampleIndexAt(m_data.startTime.msecsTo(m_data.triggerTime) / 1000.0);
    }

    // 模拟量曲线
    for (int k = 0; k < analogShown.size(); ++k) {
        const int channel = analogShown.at(k);
        const QRect band(plot.left(), analogTop + k * bandA, plot.width(), bandA);
        if (band.bottom() < plot.top() || band.top() > plot.bottom()) {
            continue;
        }
        drawAnalog(painter, channel, band, channelColor(channel), firstSample, lastSample);

        const CtAnalogChannel &info = m_data.analogs.at(channel);
        const QString label = info.id;
        const QString phasor = phasorText(channel, refSample);
        QFontMetrics fm(painter.font());
        const int nameWidth = fm.width(label);
        const int phasorWidth = phasor.isEmpty() ? 0 : fm.width(phasor) + 12;
        painter.fillRect(QRect(band.left() + 2, band.top() + 1,
                               nameWidth + phasorWidth + 8, fm.height() + 2),
                         QColor(255, 255, 255, 215));
        painter.setPen(QColor(55, 55, 60));
        painter.drawText(band.left() + 5, band.top() + 1 + fm.ascent() + 1, label);
        if (!phasor.isEmpty()) {
            painter.setPen(channelColor(channel));
            painter.drawText(band.left() + 5 + nameWidth + 12,
                             band.top() + 1 + fm.ascent() + 1, phasor);
        }

        if (bandA >= 40) {
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
        const int top = analogTop + analogShown.size() * bandA + k * bandD;
        const QRect band(plot.left(), top, plot.width(), bandD);
        if (band.bottom() < plot.top() || band.top() > plot.bottom()) {
            continue;
        }
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

    // 光标（相量参考点）
    if (m_cursorX >= plot.left() && m_cursorX <= plot.right()) {
        painter.setPen(QPen(QColor(160, 160, 168), 1, Qt::DotLine));
        painter.drawLine(m_cursorX, plot.top(), m_cursorX, plot.bottom());
    }

    // 时标 t1（左键） / t2（右键）
    if (m_t1 >= 0.0 && m_t1 >= m_viewStart && m_t1 <= m_viewEnd) {
        const int x = mapX(m_t1);
        painter.setPen(QPen(QColor(30, 90, 200), 1));
        painter.drawLine(x, plot.top(), x, plot.bottom());
        painter.setPen(QColor(30, 90, 200));
        painter.drawText(x + 3, plot.top() + 12, QStringLiteral("t1"));
    }
    if (m_t2 >= 0.0 && m_t2 >= m_viewStart && m_t2 <= m_viewEnd) {
        const int x = mapX(m_t2);
        painter.setPen(QPen(QColor(210, 110, 0), 1));
        painter.drawLine(x, plot.top(), x, plot.bottom());
        painter.setPen(QColor(210, 110, 0));
        painter.drawText(x + 3, plot.top() + 26, QStringLiteral("t2"));
    }
    if (m_t1 >= 0.0 && m_t2 >= 0.0) {
        // 两个时标之间画一条双向箭头并标注差值
        const int x1 = mapX(m_t1);
        const int x2 = mapX(m_t2);
        const int left = qMin(x1, x2);
        const int right = qMax(x1, x2);
        const int y = plot.top() + 42;
        painter.setPen(QPen(QColor(90, 92, 98), 1));
        painter.drawLine(left, y, right, y);
        painter.drawLine(left, y - 4, left, y + 4);
        painter.drawLine(right, y - 4, right, y + 4);
        const QString text = QStringLiteral("Δt = %1 ms").arg((m_t2 - m_t1) * 1000.0, 0, 'f', 3);
        painter.drawText(QRect(left + 4, y - 16, qMax(120, right - left - 8), 14), text);
    }
    painter.restore();

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

QString WaveformView::phasorText(int analogIndex, int sample) const
{
    if (analogIndex < 0 || analogIndex >= m_data.analogs.size()) {
        return QString();
    }
    const CtAnalogChannel &channel = m_data.analogs.at(analogIndex);
    if (channel.values.isEmpty() || sample < 0 || sample >= channel.values.size()
        || sample >= m_data.times.size()) {
        return QString();
    }

    const double PI = 3.14159265358979323846;
    const double frequency = m_data.lineFrequency > 0.0 ? m_data.lineFrequency : 50.0;
    const double tEnd = m_data.times.at(sample);
    const int start = qMax(0, sampleIndexAt(tEnd - 1.0 / frequency));
    const int count = sample - start + 1;
    if (count < 8) {
        return QString();
    }

    // 整工频周期 DFT：以参考采样点为相位零点，得到基波相量
    double sumCos = 0.0;
    double sumSin = 0.0;
    for (int k = start; k <= sample; ++k) {
        const double phase = 2.0 * PI * frequency * (m_data.times.at(k) - tEnd);
        sumCos += channel.values.at(k) * std::cos(phase);
        sumSin += channel.values.at(k) * std::sin(phase);
    }
    const double re = 2.0 * sumCos / count;
    const double im = 2.0 * sumSin / count;
    const double rms = std::sqrt(re * re + im * im) / std::sqrt(2.0);
    double angle = -std::atan2(im, re) * 180.0 / PI;

    const QString unit = channel.unit.isEmpty() ? QString() : channel.unit;
    return QStringLiteral("%1%2∠%3°")
        .arg(rms, 0, 'f', 3)
        .arg(unit)
        .arg(angle, 0, 'f', 3);
}

QString WaveformView::buildSelectionText() const
{
    if (m_t1 < 0.0 && m_t2 < 0.0) {
        return QString();
    }

    QStringList parts;
    if (m_t1 >= 0.0) {
        parts << QStringLiteral("t1 = %1 s").arg(m_t1, 0, 'f', 6);
    }
    if (m_t2 >= 0.0) {
        parts << QStringLiteral("t2 = %1 s").arg(m_t2, 0, 'f', 6);
    }
    if (m_t1 >= 0.0 && m_t2 >= 0.0) {
        const double delta = m_t2 - m_t1;
        parts << QStringLiteral("Δt = t2 - t1 = %1 ms").arg(delta * 1000.0, 0, 'f', 3);
    }
    return parts.join(QStringLiteral("    "));
}

void WaveformView::wheelEvent(QWheelEvent *event)
{
    if (!hasData()) {
        return;
    }

    // Ctrl+滚轮：缩放时间轴
    if (event->modifiers() & Qt::ControlModifier) {
        const double fullSpan = m_data.duration > 0.0 ? m_data.duration : 1.0;
        const double span = m_viewEnd - m_viewStart;
        const double cursorTime = timeAt(event->pos().x());
        const double factor = (event->delta() > 0) ? 0.8 : 1.25;
        const double minSpan = qMax(fullSpan * 1e-5, 1e-6);
        const double newSpan = qBound(minSpan, span * factor, fullSpan);
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
        return;
    }

    // 普通滚轮：滚动通道区垂直滑块
    if (m_vScroll->maximum() > 0) {
        const int notches = event->delta() / 120;
        const int steps = notches != 0 ? notches : (event->delta() > 0 ? 1 : -1);
        m_vScroll->setValue(m_vScroll->value() - steps * m_vScroll->singleStep());
    }
    event->accept();
}

void WaveformView::mousePressEvent(QMouseEvent *event)
{
    if (!hasData()) {
        return;
    }
    const QRect plot = plotRect();
    const int x = event->pos().x();

    if (event->button() == Qt::LeftButton) {
        // 左键：设置时标 t1，同时按住可拖动平移
        if (x >= plot.left() && x <= plot.right()) {
            m_t1 = timeAt(x);
            emit selectionInfo(buildSelectionText());
            update();
        }
        m_dragging = true;
        m_dragLastX = x;
        setCursor(Qt::ClosedHandCursor);
    } else if (event->button() == Qt::RightButton) {
        // 右键：设置时标 t2
        if (x >= plot.left() && x <= plot.right()) {
            m_t2 = timeAt(x);
            emit selectionInfo(buildSelectionText());
            update();
        }
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
}
