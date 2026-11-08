#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QHash>
#include <QPair>
#include <QByteArray>
#include <QPlainTextEdit>
#include <QTextEdit>
#include <functional>
#include "ProtocolDefine.h"
#include "msglogger.h"
#include "iprotocol.h"
#include "tcpclient.h"
#include "northchina103.h"
#include "wavemanagedialog.h"

class QTreeWidgetItem;

// 采集/初始化流程步骤（历史遗留枚举，当前实现未使用，保留原定义）
enum InitStep {
    Init_Idle,
    Step1_Sent_RCU,      // 已发复位通信单元，等待确认
    Step2_Sent_PL1_1,    // 已发第一次召唤一级数据，等待ASDU5
    Step3_Sent_RFB,      // 已发复位帧计数位，等待确认
    Step4_Sent_PL1_2,    // 已发第二次召唤一级数据，等待ASDU5
    Init_Done
};


namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT

public:
    explicit MainWindow(QWidget *parent = nullptr);
    ~MainWindow();

private slots:
    void on_treeWidget_customContextMenuRequested(const QPoint &pos);
    void on_plainTextEdit_customContextMenuRequested(const QPoint &pos);
private:
    // ========== 报文解析 ==========
    // 一条日志行的分析结果（用于整帧选中与合并解析）
    struct LogLineInfo {
        bool valid = false;         // 是否含可解析的十六进制字节
        QString caption;            // 冒号前的文字，如“04:03:20 接收”
        QString direction;          // “发送” / “接收”
        QByteArray bytes;           // 该行的报文字节
        bool startsAtFrame = false; // 首字节是否为帧起始（0x10 / 0x68）
        bool complete = false;      // 整行字节是否恰好构成完整帧序列
        bool lastFrameAcd = false;  // 末帧控制域 bit5（接收侧表示还有后续帧）
        int deviceAddr = -1;        // 末帧装置地址
    };

    // 分析一行“时间 发送/接收：16进制报文”格式的日志
    LogLineInfo analyzeLogLine(const QString &line) const;
    // 返回某行所属逻辑报文的 [起始行, 结束行]（供报文收发窗口整帧选中）
    QPair<int, int> messageGroupForBlock(int blockNumber) const;
    // 解析右键位置（或当前选中）的报文，并输出到“报文解析窗口”
    void parseLogMessagesAt(const QPoint &pos);
    // 状态栏提示：当前选中的报文共有多少字节
    void updateSelectionByteHint();
    // 按内容自动调整“报文解析”两列的默认宽度（之后仍可手动拖动）
    void autoFitParseColumns();
    // 合并字节流的来源行（用于把解析明细行映射回报文收发窗口）
    struct LogByteSource {
        int blockNumber = -1; // 报文收发窗口中的行号
        int startByte = 0;    // 该行在合并字节流中的起始字节偏移
        int byteCount = 0;    // 该行的字节数
    };
    // 把一段合并后的字节流按帧解析并输出
    void appendParsedStream(const QString &caption, int lineCount, const QByteArray &raw,
                            const QList<LogByteSource> &sources);
    // 在报文收发窗口把本次解析的报文全部标为灰色
    void applyParsedHighlight();
    // 点中解析表格某一行时，在报文收发窗口把对应字节标为绿色
    void highlightLogByteForParseRow(int row);
    // 向“报文解析窗口”追加一行说明（横跨两列）；byteStart/byteCount 记录该行对应的字节区间
    void appendParseLine(const QString &text, int byteStart = -1, int byteCount = 0);
    // 向“报文解析窗口”追加一条字节明细：原文与解析分列显示
    void appendParseDetail(const QString &original, const QString &desc,
                           int byteStart, int byteCount);

    // ========== 右键菜单：命令表 + 分发 ==========
    // 建立“菜单文字 -> 处理函数”的映射（在构造函数中调用一次）
    void initDeviceCommands();
    // 设备项右键菜单
    void showDeviceMenu(const QPoint &pos);
    // 空白处右键菜单（添加设备）
    void showBlankMenu(const QPoint &pos);
    // 当前选中的设备项（与原实现一致，使用 currentItem 而非右键命中项）
    QTreeWidgetItem *currentDeviceItem() const;

    // 以当前连接构建并发送一条规约报文；addr < 0 表示不传地址
    void sendProtocolFrame(const QString &type, int addr, const QString &errTip);

    // ========== 各菜单命令处理 ==========
    void cmdConnectDevice();
    void cmdResetCommUnit();
    void cmdResetFcb();
    void cmdGeneralInterrogation();
    void cmdSummonLevel1();
    void cmdSummonLevel2();
    void cmdTimeSyncNoop();
    void cmdOpenWaveDialog();
    void cmdEditDevice();
    void cmdDeleteDevice();

    InitStep m_initStep = Init_Idle;

    Ui::MainWindow *ui;
    // 网络
    TcpClient *m_tcp;

    // 规约
    IProtocol *m_protocol;

    // 本次解析涉及的所有日志行（用于灰色标记）
    QList<LogByteSource> m_parsedSources;
    // 多个解析段的字节流依次拼接，给每段分配独立的字节基址，避免偏移冲突
    int m_parsedByteBase = 0;
    // 报文收发窗口中“被解析报文”的灰色高亮（点击明细行时叠加绿色）
    QList<QTextEdit::ExtraSelection> m_parsedGrey;

    WaveManageDialog *m_waveDialog; // 录波文件管理页签（常驻，位于底部 tabWidget）

    // 右键菜单命令表
    QHash<QString, std::function<void()>> m_deviceCommands;
};

#endif // MAINWINDOW_H
