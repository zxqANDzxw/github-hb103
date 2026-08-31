#include "msglogger.h"
#include <QDateTime>

MsgLogger::MsgLogger(QObject *parent)
    : QObject(parent)
{
    m_logFile = new QFile(this);
    m_stream  = new QTextStream(); // 空构造，正确
}

MsgLogger::~MsgLogger()
{
    // 先关闭流
    m_stream->setDevice(nullptr);

    // 关闭文件
    if (m_logFile->isOpen()) {
        m_logFile->close();
    }

    // 一定要删除！否则内存泄漏
    delete m_stream;
    m_stream = nullptr;
}

void MsgLogger::setLogFilePath(const QString &path)
{
    // 如果已经打开，先关闭
    if (m_logFile->isOpen()) {
        m_logFile->close();
    }

    // 设置新路径并打开
    m_logFile->setFileName(path);
    bool ok = m_logFile->open(QIODevice::Append | QIODevice::Text);

    // 只有打开成功，才绑定
    if (ok) {
        m_stream->setDevice(m_logFile);
    } else {
        m_stream->setDevice(nullptr);
    }
}

void MsgLogger::writeLog(MsgDirection dir, const QString &hexStr)
{
    if (!m_logFile->isOpen() || !m_stream->device()) {
        return;
    }

    QString timeStr = QDateTime::currentDateTime().toString("yyyy-MM-dd HH:mm:ss");
    QString dirStr  = (dir == MsgDirection::Send) ? "发送" : "接收";

    // 写入日志
    *m_stream << timeStr << "  [" << dirStr << "]  " << hexStr << "\n";
    m_stream->flush(); // 强制写入磁盘
}
