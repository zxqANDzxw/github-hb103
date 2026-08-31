#include "util.h"
#include <QTime>
#include <QPlainTextEdit>
// 全局指针，指向你的日志文本框（或者传参）
extern QPlainTextEdit *g_logEdit;


QString getTimeStr()
{
    return QTime::currentTime().toString("hh:mm:ss");
}

void printSendLog(QByteArray sendData, QPlainTextEdit* logWidget)
{
    if(!logWidget) return; // 空指针保护
    QString hex = sendData.toHex(' ').toUpper();
    QString log = QString("%1 发送：%2").arg(getTimeStr(), hex);
    logWidget->appendPlainText(log);
}


void printRecvLog(QByteArray recvData, QPlainTextEdit* logWidget)
{
    if(!logWidget) return;
    QString hex = recvData.toHex(' ').toUpper();
    QString log = QString("%1 接收是这里么？？：%2").arg(getTimeStr(), hex);
    logWidget->appendPlainText(log);
}
