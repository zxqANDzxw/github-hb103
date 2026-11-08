#ifndef PARSEDTABLEWIDGET_H
#define PARSEDTABLEWIDGET_H

#include <QTableWidget>
#include <QHeaderView>
#include <QPainter>
#include <QPen>
#include <QColor>

// 报文解析窗口：单列表格，一行一条解析信息。
// 用表格而不是 QTextEdit，是为了让“整行选中”由控件原生保证（SelectRows），
// 同时在每行下方画一条红色细线，保留信纸式阅读效果。
class ParsedTableWidget : public QTableWidget
{
public:
    explicit ParsedTableWidget(QWidget *parent = nullptr)
        : QTableWidget(parent)
        , m_ruleColor(200, 60, 60)
    {
        // 两列：左“报文原文”放偏移 + 字节，右“报文解析”放字段说明
        setColumnCount(2);
        setHorizontalHeaderLabels(QStringList() << QStringLiteral("报文原文")
                                                << QStringLiteral("报文解析"));
        horizontalHeader()->setVisible(true);
        // 列宽由主窗口按内容计算（见 MainWindow::autoFitParseColumns），
        // 同时保持 Interactive，用户可拖动分隔线手动调整
        horizontalHeader()->setStretchLastSection(false);
        horizontalHeader()->setSectionResizeMode(QHeaderView::Interactive);
        verticalHeader()->setVisible(false);

        // 整行选中 + 单选，不可编辑
        setSelectionBehavior(QAbstractItemView::SelectRows);
        setSelectionMode(QAbstractItemView::SingleSelection);
        setEditTriggers(QAbstractItemView::NoEditTriggers);
        setShowGrid(false);
        setWordWrap(true);
        setAlternatingRowColors(false);
        setVerticalScrollMode(QAbstractItemView::ScrollPerPixel);

        // 行高随内容自动调整
        verticalHeader()->setSectionResizeMode(QHeaderView::ResizeToContents);

        QFont f(QStringLiteral("SimSun"), 9);
        setFont(f);

        // 选中行使用浅灰底（与 IDE 列表的选中效果一致），并保证整行铺满
        setStyleSheet(QStringLiteral(
            "QTableWidget { background: #FFFFFF; }"
            "QTableWidget::item:selected { background: #E6E8EB; color: #000000; }"
            "QHeaderView::section { background: #F2F3F5; color: #333333;"
            " border: none; border-bottom: 1px solid #D5D7DB;"
            " padding: 2px 4px; }"));
    }

protected:
    void paintEvent(QPaintEvent *event) override
    {
        QTableWidget::paintEvent(event);
        if (rowCount() <= 0) {
            return;
        }

        // 每行底部画红色横线，模拟信纸
        QPainter painter(viewport());
        painter.setPen(QPen(m_ruleColor, 1));
        const QRect clip = event->rect();
        for (int r = 0; r < rowCount(); ++r) {
            const int top = rowViewportPosition(r);
            if (top > clip.bottom()) {
                break;
            }
            const int bottom = top + rowHeight(r) - 1;
            if (bottom < clip.top()) {
                continue;
            }
            painter.drawLine(clip.left(), bottom, clip.right(), bottom);
        }
    }

private:
    QColor m_ruleColor;
};

#endif // PARSEDTABLEWIDGET_H
