#ifndef UTIL_H
#define UTIL_H


#include <QString>
#include <QTime>

class QPlainTextEdit; // 前置声明，不用include头文件

// 获取 当前 时:分:秒 时间字符串
QString getTimeStr();


// 把文本框指针作为入参传入
void printSendLog(QByteArray sendData, QPlainTextEdit* logWidget);

void printRecvLog(QByteArray recvData, QPlainTextEdit* logWidget);

#endif // UTIL_H
