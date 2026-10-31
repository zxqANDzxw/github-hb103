#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include <QHash>
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
    void on_plainTextEdit_cursorPositionChanged();
private:
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

    WaveManageDialog *m_waveDialog; // 录波对话框长效指针

    // 右键菜单命令表
    QHash<QString, std::function<void()>> m_deviceCommands;
};

#endif // MAINWINDOW_H
