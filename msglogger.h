#ifndef MSGLOGGER_H
#define MSGLOGGER_H

#include <QObject>
#include <QFile>
#include <QTextStream>
#include "ProtocolDefine.h"

class MsgLogger : public QObject
{
    Q_OBJECT
public:
    explicit MsgLogger(QObject *parent = nullptr);
    ~MsgLogger();

    // 设置日志文件路径
    void setLogFilePath(const QString& path);
    // 写入一条报文日志
    void writeLog(MsgDirection dir, const QString& hexStr);

private:
    QFile* m_logFile = nullptr;
    QTextStream* m_stream = nullptr;
};

#endif // MSGLOGGER_H
