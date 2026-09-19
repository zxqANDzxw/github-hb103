#include "northchina103.h"
#include <QDateTime>
#include <QTimer>
#include <QDebug>

NorthChina103::NorthChina103(QObject *parent)
    : IProtocol(parent)
    , m_fcb(false)
    , m_pollTimer(new QTimer(this))
{
    // 二级数据轮询：默认1秒，超时自动组帧并请求发送
    m_pollTimer->setInterval(10000);
    connect(m_pollTimer, &QTimer::timeout, this, [=](){
        emit requestSend(buildFrame({{"type", "召唤二级数据"}, {"addr", 0x01}}));
    });
}

void NorthChina103::startPoll()
{
    if (m_pollTimer && !m_pollTimer->isActive()) {
        m_pollTimer->start();
    }
}

void NorthChina103::stopPoll()
{
    if (m_pollTimer && m_pollTimer->isActive()) {
        m_pollTimer->stop();
    }
}



NorthChina103::~NorthChina103()
{
}

// ==============================
// 基础工具函数
// ==============================

// 固定帧校验和：控制域 ^ 地址
uchar NorthChina103::calcFixedFrameCS(uchar ctrl, uchar addr)
{
    return ctrl ^ addr;
}

// CP56Time2a 编码：QDateTime → 7字节
QByteArray NorthChina103::cp56FromDateTime(const QDateTime &dt)
{
    QByteArray bytes(7, Qt::Uninitialized);
        quint16 ms = dt.time().msec() + dt.time().second() * 1000;
        bytes[0] = ms & 0xFF;
        bytes[1] = (ms >> 8) & 0xFF;
        bytes[2] = dt.time().minute();

        // 修复：小时 + 星期位（高3位）
        int hour = dt.time().hour();
        int weekDay = dt.date().dayOfWeek(); // Qt:1=周一 ~ 7=周日，和规约一致
        uchar hourByte = hour & 0x1F;
        hourByte |= (weekDay << 5) & 0xE0;
        bytes[3] = hourByte;

        bytes[4] = dt.date().day();
        bytes[5] = dt.date().month();
        bytes[6] = dt.date().year() % 100;
        return bytes;
}

// CP56Time2a 解码：7字节 → QDateTime
QDateTime NorthChina103::cp56ToDateTime(const QByteArray &bytes)
{
    if (bytes.size() != 7) return QDateTime();
    quint16 ms = (quint8)bytes[0] | ((quint8)bytes[1] << 8);
    int min = bytes[2] & 0x3F;
    int hour = bytes[3] & 0x1F;
    int day = bytes[4] & 0x1F;
    int month = bytes[5] & 0x0F;
    int year = 2000 + (bytes[6] & 0x7F);
    return QDateTime(QDate(year, month, day), QTime(hour, min, ms / 1000, ms % 1000));
}

QString NorthChina103::protocolName() const
{
    return "华北电网103 TCP";
}

// ==============================
// 帧解析（报文详情展示用）
// ==============================

ProtocolParseResult NorthChina103::parseFrame(const QByteArray &frame)
{
    ProtocolParseResult res;
    res.ok = true;
    res.rawHex = frame.toHex(' ').toUpper();

    if (frame.size() < 5) {
        res.detailDesc = "报文长度不足";
        return res;
    }

    uchar head = frame[0];

    // ---------- 固定长度帧（5字节） ----------
    if (head == 0x10 && frame.size() == 5) {
        uchar ctrl  = frame[1];
        uchar addr  = frame[2];
        uchar cs    = frame[3];
        uchar tail  = frame[4];

        res.fieldList << qMakePair(QStringLiteral("启动符"), QString("%1").arg(head, 2, 16, QChar('0')));
        res.fieldList << qMakePair(QStringLiteral("控制域"), QString("%1").arg(ctrl, 2, 16, QChar('0')));
        res.fieldList << qMakePair(QStringLiteral("地址域"), QString("%1").arg(addr, 2, 16, QChar('0')));
        res.fieldList << qMakePair(QStringLiteral("校验和"), QString("%1").arg(cs, 2, 16, QChar('0')));
        res.fieldList << qMakePair(QStringLiteral("结束符"), QString("%1").arg(tail, 2, 16, QChar('0')));
        res.detailDesc = "华北103 固定长度帧";
        return res;
    }

    // ---------- 可变长度帧（68开头） ----------
    if (head == 0x68 && frame.size() >= 6) {
        uchar lenL  = frame[1];
        uchar lenH  = frame[2];
        uchar ctrl  = frame[4];
        uchar addr  = frame[5];
        uchar cs    = frame[frame.size()-2];
        uchar tail  = frame[frame.size()-1];

        res.fieldList << qMakePair(QStringLiteral("启动符"), QString("%1").arg(head, 2, 16, QChar('0')));
        res.fieldList << qMakePair(QStringLiteral("长度低"), QString("%1").arg(lenL, 2, 16, QChar('0')));
        res.fieldList << qMakePair(QStringLiteral("长度高"), QString("%1").arg(lenH, 2, 16, QChar('0')));
        res.fieldList << qMakePair(QStringLiteral("控制域"), QString("%1").arg(ctrl, 2, 16, QChar('0')));
        res.fieldList << qMakePair(QStringLiteral("地址域"), QString("%1").arg(addr, 2, 16, QChar('0')));
        res.fieldList << qMakePair(QStringLiteral("校验和"), QString("%1").arg(cs, 2, 16, QChar('0')));
        res.fieldList << qMakePair(QStringLiteral("结束符"), QString("%1").arg(tail, 2, 16, QChar('0')));
        res.detailDesc = "华北103 可变长度帧";
        return res;
    }

    res.detailDesc = "非标准103帧格式";
    return res;
}

// ==============================
// 帧组装
// ==============================

QByteArray NorthChina103::buildFrame(const QVariantMap &param)
{
    QString type = param.value("type").toString();
    uchar addr = (uchar)param.value("addr", 0x01).toInt();

    qDebug() << "当前type:" << type;


    // ---------- 固定长度帧 ----------
    if (type == "复位通信单元") {
        return buildFixedFrame(0x40, addr);
    }
    else if (type == "复位帧计数位") {
        uchar ctrl = 0x47;
        if (m_fcb) ctrl |= 0x20;
        m_fcb = !m_fcb;
        return buildFixedFrame(ctrl, addr);
    }
    else if (type == "召唤一级数据") {
        uchar ctrl = 0x5A;
        if (m_fcb) ctrl |= 0x20;
        m_fcb = !m_fcb;
        return buildFixedFrame(ctrl, addr);
    }
    else if (type == "召唤二级数据") {
        uchar ctrl = 0x5B;
        if (m_fcb) ctrl |= 0x20;
        m_fcb = !m_fcb;
        return buildFixedFrame(ctrl, addr);
    }
    else if (type == "装置对时") {
        return QByteArray::fromHex("10 67 01 66 16");
    }
    else if (type == "信号复归") {
        return QByteArray::fromHex("10 68 01 69 16");
    }
    else if (type == "召唤录波列表") {
        uchar ctrl = 0x08;
        if (m_fcb) ctrl |= 0x20;
        m_fcb = !m_fcb;
        return buildFixedFrame(ctrl, addr);
    }

    // ---------- 可变长度帧 ----------
    else if (type == "总召唤") {
        return QByteArray::fromHex("68 09 09 68 53 01 64 01 01 01 00 00 00 67 16");
    }
    else if (type == "召唤录波列表_按时间") {
        QDateTime startTime = param.value("startTime").toDateTime();
        QDateTime endTime = param.value("endTime").toDateTime();

        QByteArray asduBody;
        asduBody.append((char)0x0F);   // TYP=0FH → ASDU15
        asduBody.append((char)0x81);   // VSQ=81H
        asduBody.append((char)0x00);   // COT
        asduBody.append((char)0x01);   // ASDU地址
        asduBody.append((char)0xFF);   // FUN
        asduBody.append((char)0x00);   // INF
        asduBody.append(cp56FromDateTime(startTime));
        asduBody.append(cp56FromDateTime(endTime));

        return buildVariableFrame(asduBody, addr);
    }

    return QByteArray();
}

// 固定帧通用封装
QByteArray NorthChina103::buildFixedFrame(uchar ctrl, uchar addr)
{
    QByteArray frame;
    frame.append((char)0x10);
    frame.append((char)ctrl);
    frame.append((char)addr);
    frame.append((char)calcFixedFrameCS(ctrl, addr));
    frame.append((char)0x16);
    return frame;
}

// 可变帧通用封装
QByteArray NorthChina103::buildVariableFrame(const QByteArray &asduBody, uchar addr)
{
    QByteArray frame;
    frame.append((char)0x68);
    // 修复后（正确：小端序，分高低字节）
    quint16 totalLen = 2 + asduBody.size();  // 控制域1 + 地址域1 + ASDU体
    frame.append((char)(totalLen & 0xFF));       // 低8位
    frame.append((char)((totalLen >> 8) & 0xFF));// 高8位
    frame.append((char)0x68);

    // 控制域：主站发送，FCB位复用全局逻辑
    uchar ctrl = 0x40 | 0x01;
    if (m_fcb) ctrl |= 0x20;
    m_fcb = !m_fcb;
    frame.append((char)ctrl);

    frame.append((char)addr);
    frame.append(asduBody);

    // 帧校验和
    uchar cs = 0;
    for (int i = 4; i < frame.size(); ++i) {
        cs += (uchar)frame[i];
    }
    frame.append((char)cs);
    frame.append((char)0x16);

    return frame;
}

// ==============================
// 拆帧
// ==============================

QList<QByteArray> NorthChina103::splitRawFrame(const QByteArray &data)
{
    int dummy = 0;
    return splitRawFrameWithCache(data, dummy);
}

QList<QByteArray> NorthChina103::splitRawFrameWithCache(const QByteArray &data, int &usedLen)
{
    usedLen = 0;
    QList<QByteArray> frames;
    int total = data.size();
    int pos = 0;

    while (pos < total)
    {
        uchar b = data[pos];

        // 固定帧 0x10 开头
        if (b == 0x10) {
            if (pos + 5 <= total && (uchar)data[pos+4] == 0x16) {
                frames.append(data.mid(pos, 5));
                pos += 5;
                continue;
            }
            break;
        }

        // 可变帧 0x68 开头
        if (b == 0x68) {
            if (pos + 4 > total) break;
            if ((uchar)data[pos+3] != 0x68) {
                pos++;
                continue;
            }

            uchar LL = data[pos+1];
            uchar HH = data[pos+2];
            quint16 bodyLen = (quint16)((HH << 8) | LL);
            int frameLen = 4 + bodyLen + 2; // 帧头 + 体 + 校验+结束符

            if (pos + frameLen > total) break;
            if ((uchar)data[pos + frameLen - 1] == 0x16) {
                frames.append(data.mid(pos, frameLen));
            }
            pos += frameLen;
            continue;
        }

        // 非法字节跳过
        pos++;
    }

    usedLen = pos;
    return frames;
}

// ==============================
// 数据入口与业务处理
// ==============================

void NorthChina103::feedRawData(const QByteArray &data)
{
    m_recvBuf.append(data);

    // 缓存溢出保护
    if (m_recvBuf.size() > 2048) {
        m_recvBuf.clear();
  //      qDebug() << "[NorthChina103] 接收缓存超限，清空脏数据";
        return;
    }

    int usedLen = 0;
    QList<QByteArray> frames = splitRawFrameWithCache(m_recvBuf, usedLen);
    m_recvBuf = m_recvBuf.mid(usedLen);

    bool hasAcd = false;

    for (const auto &frame : frames)
    {
        // 全帧判断ACD位
        if (frame.size() == 5 && frame[0] == 0x10) {
            if (((uchar)frame[1] & 0x20) != 0) hasAcd = true;
        }
        else if (frame[0] == 0x68 && frame.size() >= 5) {
            if (((uchar)frame[4] & 0x20) != 0) hasAcd = true;
        }

        // 只处理可变帧的ASDU业务
        if ((uchar)frame[0] != 0x68) continue;
        if (frame.size() < 7) continue;

        uchar asduType = frame[6];

        // ASDU12 = 录波文件列表
        if (asduType == 0x0C) {
            QList<WaveFileInfo> list = parseWaveListAsdu(frame);
            if (!list.isEmpty()) {
                emit waveListReceived(list);
            }
        }

        // 后续扩展其他ASDU...
    }

    // 整批处理完统一响应一级召唤
    if (hasAcd) {
        emit requestSend(buildFrame({{"type","召唤一级数据"},{"addr",0x01}}));
    }
}

// 召唤录波文件列表（对外接口）
void NorthChina103::callWaveFileList(const QDateTime &startTime, const QDateTime &endTime)
{
    m_tempFileList.clear();
    QVariantMap param;
    param["type"] = "召唤录波列表_按时间";
    param["addr"] = 0x01;//这里没有用到这个参数
    param["startTime"] = startTime;
    param["endTime"] = endTime;

    QByteArray frame = buildFrame(param);
    if (!frame.isEmpty()) { emit requestSend(frame); }

}

// 解析ASDU12录波列表
QList<WaveFileInfo> NorthChina103::parseWaveListAsdu(const QByteArray &frame)
{
    QList<WaveFileInfo> result;
    if (frame.size() < 23) return result; // 帧头(6) + 信息头(10) + 7字节时间 = 23

    WaveFileInfo info;
    int infoStart = 16; // 信息体起始偏移，按实际规约调整

    // 故障时间
    info.faultTime = cp56ToDateTime(frame.mid(infoStart, 7)).toString("yyyy-MM-dd HH:mm:ss.zzz");
    // 文件名（40字节，去尾部填充）
    QByteArray nameBytes = frame.mid(infoStart + 7, 40);
    info.fileName = QString::fromLatin1(nameBytes).remove(QChar('\0'));
    info.fileSize = 0;

    result.append(info);
    return result;
}

// ==============================
// 缓存与轮询接口
// ==============================

QByteArray NorthChina103::getRecvBuf() const
{
    return m_recvBuf;
}

void NorthChina103::setRecvBuf(const QByteArray &buf)
{
    m_recvBuf = buf;
}

void NorthChina103::clearRecvBuf()
{
    m_recvBuf.clear();
}


