#include "northchina103.h"
#include <QDateTime>
#include <QTimer>
#include <QDebug>

NorthChina103::NorthChina103(QObject *parent)
    : IProtocol(parent)
    , m_fcb(false)
    , m_pollTimer(new QTimer(this))
    , m_waveTransTimer(nullptr)
    , m_isWaveTransmitting(false)
    , m_isFileDownloading(false)
    , m_fileDownloadTimer(nullptr)

{
    // 二级数据轮询：默认1秒，超时自动组帧并请求发送
    m_pollTimer->setInterval(10000);
    connect(m_pollTimer, &QTimer::timeout, this, [=](){
        emit requestSend(buildFrame({{"type", "召唤二级数据"}, {"addr", 0x01}}));
    });


    m_waveTransTimer = new QTimer(this);
        m_waveTransTimer->setSingleShot(true);
        connect(m_waveTransTimer, &QTimer::timeout, this, [=](){
            // 超时兜底：清空缓存，重置状态，上报错误
            m_waveListCache.clear();
            m_isWaveTransmitting = false;
            emit waveListError("文件列表传输超时");
            m_pollTimer->start(); // 恢复二级数据轮询
        });

        // 文件下载超时定时器
            m_fileDownloadTimer = new QTimer(this);
            m_fileDownloadTimer->setSingleShot(true);
            connect(m_fileDownloadTimer, &QTimer::timeout, this, [=](){
               m_filePackageCache.clear();
                m_isFileDownloading = false;
                emit waveDownloadError("录波文件下载超时");
                m_pollTimer->start(); // 恢复轮询
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
        bytes[0] = ms & 0xFF;          // 字节1：毫秒低8位
        bytes[1] = (ms >> 8) & 0xFF;   // 字节2：毫秒高8位

        // 字节3：分钟（低6位），IV和RES位默认0
        bytes[2] = dt.time().minute() & 0x3F;

        // 字节4：小时（低5位），SU和RES位默认0
        bytes[3] = dt.time().hour() & 0x1F;

        // 字节5：高3位=星期，低5位=日期
        int weekDay = dt.date().dayOfWeek(); // Qt:1=周一 ~ 7=周日
        bytes[4] = (dt.date().day() & 0x1F) | ((weekDay << 5) & 0xE0);

        // 字节6：月份（低4位），高4位RES=0
        bytes[5] = dt.date().month() & 0x0F;

        // 字节7：年份（低7位），最高位RES=0
        bytes[6] = (dt.date().year() % 100) & 0x7F;
        return bytes;
}

// CP56Time2a 解码：7字节 → QDateTime
QDateTime NorthChina103::cp56ToDateTime(const QByteArray &bytes)
{
    if (bytes.size() != 7) return QDateTime();

        quint16 ms = static_cast<quint8>(bytes[0])
                   | (static_cast<quint8>(bytes[1]) << 8);
        int minute = static_cast<quint8>(bytes[2]) & 0x3F;
        int hour   = static_cast<quint8>(bytes[3]) & 0x1F;
        int day    = static_cast<quint8>(bytes[4]) & 0x1F;
        int month  = static_cast<quint8>(bytes[5]) & 0x0F;
        int year   = 2000 + (static_cast<quint8>(bytes[6]) & 0x7F);

        return QDateTime(QDate(year, month, day), QTime(hour, minute, ms / 1000, ms % 1000));
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

void NorthChina103::downloadWaveFile(const QString &fileName, quint32 startPos)
{

    qDebug() << "[1] 进入下载函数";
        if (m_isFileDownloading) return;

        qDebug() << "[2] 暂停二级轮询";
        m_pollTimer->stop();

        qDebug() << "[3] 重置下载状态";
        m_downloadFileName = fileName;
        m_filePackageCache.clear();
        m_isFileDownloading = true;

        qDebug() << "[4] 定时器地址：" << m_fileDownloadTimer;
        m_fileDownloadTimer->start(8000); // 首帧超时8秒

        qDebug() << "[5] 开始组装ASDU";
        QByteArray asdu;
        asdu.append(char(0x0D));
        asdu.append(char(0x81));
        asdu.append(char(0x00));
        asdu.append(char(0x01));
        asdu.append(char(0xFF));
        asdu.append(char(0x00));

        qDebug() << "[6] 填充文件名";
        QByteArray nameBytes = fileName.toLatin1();
        nameBytes.resize(40);
        nameBytes.fill('\0');
        asdu.append(nameBytes);

        qDebug() << "[7] 填充起始传输位置";
        asdu.append(char(startPos & 0xFF));
        asdu.append(char((startPos >> 8) & 0xFF));
        asdu.append(char((startPos >> 16) & 0xFF));
        asdu.append(char((startPos >> 24) & 0xFF));

        qDebug() << "[8] 调用可变帧组帧";
        QByteArray frame = buildVariableFrame(asdu, 0x01);
        qDebug() << "[9] 组帧完成，长度：" << frame.size();

        if (!frame.isEmpty()) {
            qDebug() << "[10] 发出requestSend信号";
            emit requestSend(frame);
        }
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

    // 控制域：应用层可变帧标准格式 PRM=1 + FCV=1 + 功能码3(请求/响应)
    uchar ctrl = 0x53;
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

        // ASDU16 = 录波文件列表
        if (asduType == 0x10) {
            QList<WaveFileInfo> list = parseWaveListAsdu(frame);

            // 2. 追加到全局缓存
            m_waveListCache.append(list);

            // 3. 读取控制域，判断ACD位
                uchar ctrl = static_cast<uchar>(frame.at(4));
                bool hasMoreFrame = (ctrl & 0x20) != 0; // bit5 = ACD位

                if (!hasMoreFrame) {
                        // 最后一帧：发出完整列表信号，清空状态
                     qDebug() << "[调试] 录波列表传输完成，总条目数:" << m_waveListCache.size(); // 加这行
                        emit waveListReceived(m_waveListCache);
                        m_waveListCache.clear();
                        m_isWaveTransmitting = false;
                        m_waveTransTimer->stop();
                        m_pollTimer->start(); // 恢复二级数据轮询

                    } else {
                        // 重置超时定时器（3秒没收到下一帧就算超时）
                        m_waveTransTimer->start(3000);
                    }


        }
        // ASDU14 = 录波文件数据应答
        else if (asduType == 0x0E) {
            if (!m_isFileDownloading) break;

                // 重置超时定时器
                m_fileDownloadTimer->start(5000);

                // ASDU内部偏移：跳过TYP+VSQ+COT+ASDU地址 = 4字节
                int asduPos = 4;

                // 1. 读取本段起始传输位置（4字节小端）
                quint32 offset = 0;
                offset |= quint8(frame[asduPos++]);
                offset |= quint8(frame[asduPos++]) << 8;
                offset |= quint8(frame[asduPos++]) << 16;
                offset |= quint8(frame[asduPos++]) << 24;

                // 2. 提取本段文件数据（到帧尾校验和之前）
                int dataLen = frame.size() - 2 - (6 + asduPos); // 链路层6 + asdu头 + 校验和结束符2
                QByteArray segment = frame.mid(6 + asduPos, dataLen);

                // 3. 按偏移写入总缓存（支持断点续传、乱序补全）
                if (offset + segment.size() > quint32(m_filePackageCache.size())) {
                    m_filePackageCache.resize(offset + segment.size());
                }
                memcpy(m_filePackageCache.data() + offset, segment.constData(), segment.size());

                // 4. 进度上报
                emit waveDownloadProgress(m_filePackageCache.size(), 0);

                // 5. 判断ACD位，是否还有后续帧
                uchar ctrl = quint8(frame.at(4));
                bool hasMore = (ctrl & 0x20) != 0;

                if (!hasMore) {
                    // 传输完成：解析拆分三个COMTRADE文件
                    QByteArray hdr, cfg, dat;
                    bool parseOk = parseComtradePackage(m_filePackageCache, hdr, cfg, dat);

                    m_isFileDownloading = false;
                    m_fileDownloadTimer->stop();
                    m_pollTimer->start(); // 恢复二级轮询

                    quint32 total = m_filePackageCache.size();
                    emit waveDownloadProgress(total, total);

                    if (parseOk) {
                        emit waveDownloadFinished(m_downloadFileName, hdr, cfg, dat);
                    } else {
                        emit waveDownloadError("文件包解析失败，格式不符合规约");
                    }
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
    // 暂停二级数据轮询，避免FCB位冲突打断录波传输
    m_pollTimer->stop();

    // 重置传输状态，清空历史缓存
    m_waveListCache.clear();
    m_isWaveTransmitting = true;
    m_waveTransTimer->start(5000); // 首帧超时5秒

    m_tempFileList.clear();
    QVariantMap param;
    param["type"] = "召唤录波列表_按时间";
    param["addr"] = 0x01;//这里没有用到这个参数
    param["startTime"] = startTime;
    param["endTime"] = endTime;

    QByteArray frame = buildFrame(param);
    if (!frame.isEmpty()) { emit requestSend(frame); }

}

// 解析ASDU16录波列表
QList<WaveFileInfo> NorthChina103::parseWaveListAsdu(const QByteArray &frame)
{
    QList<WaveFileInfo> result;
        // 最小帧长度校验：链路层6 + ASDU头20 + 至少1个条目48 = 74
        if (frame.size() < 74)
            return result;

        // ========== 1. 定位ASDU，读取本帧文件数 ==========
        int asduStart = 6; // 跳过链路层6字节
        // 本帧文件数：ASDU第4~5字节，小端序
        quint16 fileCount = static_cast<quint8>(frame[asduStart + 4])
                          | (static_cast<quint8>(frame[asduStart + 5]) << 8);

        // ========== 2. 定位第一个文件条目 ==========
        int pos = asduStart + 20; // 跳过20字节ASDU头部，指向第一个装置地址

        // ========== 3. 循环解析每个文件 ==========
        for (quint16 i = 0; i < fileCount; ++i) {
            if (pos + 48 > frame.size()) // 每个条目固定48字节
                break;

            WaveFileInfo info;

            // 1) 录波装置地址（1字节）
            info.deviceAddr = static_cast<quint8>(frame[pos]);
            pos += 1;

            // 2) 文件名：固定40字节，去掉尾部补0
            QByteArray nameRaw = frame.mid(pos, 40);
            info.fileName = QString::fromLatin1(nameRaw).remove(QChar('\0'));
            pos += 40;

            // 3) 故障时间：7字节CP56Time2a
            info.faultTime = cp56ToDateTime(frame.mid(pos, 7));
            pos += 7;

            info.fileSize = 0;
            result.append(info);
        }

        return result;
}
void NorthChina103::requestLevel1Data()
{
    emit requestSend(buildFrame({{"type","召唤一级数据"},{"addr",0x01}}));
}

bool NorthChina103::parseComtradePackage(const QByteArray &pkg,QByteArray &outHdr,QByteArray &outCfg, QByteArray &outDat)
{
    int pos = 0;

        // 1. 跳过40字节文件名
        if (pkg.size() < 47) return false;
        pos += 40;

        // 2. 跳过7字节故障时间（如需使用可自行解析）
        pos += 7;

        // 3. 依次解析3个文件块
        for (int i = 0; i < 3; ++i) {
            if (pos + 5 > pkg.size()) return false;

            // 文件类型：1=HDR 2=CFG 3=DAT
            quint8 type = quint8(pkg[pos++]);

            // 文件长度：4字节小端
            quint32 len = 0;
            len |= quint8(pkg[pos++]);
            len |= quint8(pkg[pos++]) << 8;
            len |= quint8(pkg[pos++]) << 16;
            len |= quint8(pkg[pos++]) << 24;

            if (pos + int(len) > pkg.size()) return false;
            QByteArray content = pkg.mid(pos, len);
            pos += len;

            switch (type) {
                case 1: outHdr = content; break;
                case 2: outCfg = content; break;
                case 3: outDat = content; break;
                default: return false;
            }
        }
        return true;
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


