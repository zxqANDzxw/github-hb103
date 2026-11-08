#ifndef FRAMELOGEDIT_H
#define FRAMELOGEDIT_H

#include <QPlainTextEdit>
#include <QMouseEvent>
#include <QTextCursor>
#include <QTextBlock>
#include <QTextDocument>
#include <QPair>
#include <functional>

// 报文收发窗口：鼠标选择时按“整行”吸附，并自动把同一条逻辑报文的
// 多行（TCP 分片 / 子站分帧上送）整体选中
class FrameLogEdit : public QPlainTextEdit
{
public:
    typedef std::function<QPair<int, int>(int)> GroupResolver;

    explicit FrameLogEdit(QWidget *parent = nullptr)
        : QPlainTextEdit(parent)
    {
    }

    // 由主窗口提供：输入行号，返回该行所属逻辑报文的 [起始行, 结束行]
    void setGroupResolver(const GroupResolver &resolver)
    {
        m_resolver = resolver;
    }

protected:
    void mousePressEvent(QMouseEvent *event) override
    {
        QPlainTextEdit::mousePressEvent(event);
        if (event->button() != Qt::LeftButton) {
            return;
        }
        if (textCursor().hasSelection()) {
            snapSelectionToBlocks();
        } else {
            selectCurrentBlock();
        }
        expandSelectionToGroup();
    }

    void mouseMoveEvent(QMouseEvent *event) override
    {
        QPlainTextEdit::mouseMoveEvent(event);
        if (!(event->buttons() & Qt::LeftButton)) {
            return;
        }
        // 拖拽过程中只吸附整行，避免选区反复跳变
        snapSelectionToBlocks();
    }

    void mouseReleaseEvent(QMouseEvent *event) override
    {
        QPlainTextEdit::mouseReleaseEvent(event);
        if (event->button() != Qt::LeftButton) {
            return;
        }
        snapSelectionToBlocks();
        expandSelectionToGroup();
    }

    void mouseDoubleClickEvent(QMouseEvent *event) override
    {
        QPlainTextEdit::mouseDoubleClickEvent(event);
        if (event->button() != Qt::LeftButton) {
            return;
        }
        snapSelectionToBlocks();
        expandSelectionToGroup();
    }

private:
    void selectCurrentBlock()
    {
        QTextBlock block = textCursor().block();
        if (!block.isValid() || block.length() <= 1) {
            return;
        }
        QTextCursor cursor(document());
        cursor.setPosition(block.position());
        cursor.setPosition(block.position() + block.length() - 1, QTextCursor::KeepAnchor);
        setTextCursor(cursor);
    }

    // 把选区两端吸附到整行（块）边界
    void snapSelectionToBlocks()
    {
        QTextCursor cursor = textCursor();
        if (!cursor.hasSelection()) {
            selectCurrentBlock();
            return;
        }

        QTextBlock startBlock = document()->findBlock(cursor.selectionStart());
        QTextBlock endBlock = document()->findBlock(cursor.selectionEnd());
        if (!startBlock.isValid() || !endBlock.isValid()) {
            return;
        }

        QTextCursor snapped(document());
        snapped.setPosition(startBlock.position());
        snapped.setPosition(endBlock.position() + endBlock.length() - 1, QTextCursor::KeepAnchor);
        setTextCursor(snapped);
    }

    // 按主窗口给出的逻辑报文范围扩展选区
    void expandSelectionToGroup()
    {
        if (!m_resolver) {
            return;
        }
        QTextCursor cursor = textCursor();
        if (!cursor.hasSelection()) {
            return;
        }

        int first = document()->findBlock(cursor.selectionStart()).blockNumber();
        int last = document()->findBlock(cursor.selectionEnd()).blockNumber();
        int lo = first;
        int hi = last;

        const QPair<int, int> gFirst = m_resolver(first);
        if (gFirst.first >= 0) {
            lo = qMin(lo, gFirst.first);
        }
        if (gFirst.second >= 0) {
            hi = qMax(hi, gFirst.second);
        }

        const QPair<int, int> gLast = m_resolver(last);
        if (gLast.first >= 0) {
            lo = qMin(lo, gLast.first);
        }
        if (gLast.second >= 0) {
            hi = qMax(hi, gLast.second);
        }

        QTextBlock loBlock = document()->findBlockByNumber(lo);
        QTextBlock hiBlock = document()->findBlockByNumber(hi);
        if (!loBlock.isValid() || !hiBlock.isValid()) {
            return;
        }

        QTextCursor expanded(document());
        expanded.setPosition(loBlock.position());
        expanded.setPosition(hiBlock.position() + hiBlock.length() - 1, QTextCursor::KeepAnchor);
        setTextCursor(expanded);
        ensureCursorVisible();
    }

    GroupResolver m_resolver;
};

#endif // FRAMELOGEDIT_H
