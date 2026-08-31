#ifndef PROTOCOLDEFINE_H
#define PROTOCOLDEFINE_H

#include <QString>
#include <QList>
#include <QPair>
#include <QByteArray>

// 单条解析结果统一结构（所有规约通用）
struct ProtocolParseResult
{
    bool ok = false;
    QString rawHex;                // 原始16进制报文
    QList<QPair<QString, QString>> fieldList; // 字段名-字段值
    QString detailDesc;            // 字段说明、含义
};

// 报文方向
enum class MsgDirection
{
    Recv,   // 接收
    Send    // 发送
};


#endif // PROTOCOLDEFINE_H
