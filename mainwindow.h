#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QTimer>
#include "ProtocolDefine.h"
#include "msglogger.h"
#include "iprotocol.h"
#include "tcpclient.h"
#include "northchina103.h"

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
    InitStep m_initStep = Init_Idle;

    Ui::MainWindow *ui;
    // 网络
    TcpClient *m_tcp;

    // 规约
    IProtocol *m_protocol;

    // 日志
    //MsgLogger *m_logger;

    // 缓存所有报文（用于点击解析）
    //QList<QByteArray> m_msgCache;
};

#endif // MAINWINDOW_H
