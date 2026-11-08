#include "northchina103.h"

#include <QDate>
#include <QTime>
#include <QTimer>
#include <QStringList>
#include <cstring>

/* ============================================================
 * 文件内常量与通用小工具
 * ============================================================ */
namespace {

/* ---------- 链路层 ---------- */
const quint8 H103_START_FIXED   = 0x10;
const quint8 H103_START_VAR     = 0x68;
const quint8 H103_END           = 0x16;

/* ---------- 控制域 ---------- */
const quint8 CTRL_RESET_CU      = 0x40; // 复位通信单元
const quint8 CTRL_RESET_FCB     = 0x47; // 复位帧计数位
const quint8 CTRL_SUM_LEVEL1    = 0x5A; // 召唤一级数据
const quint8 CTRL_SUM_LEVEL2    = 0x5B; // 召唤二级数据
const quint8 CTRL_WAVE_LIST     = 0x08; // 召唤录波列表
const quint8 CTRL_VAR_FRAME     = 0x53; // 可变帧默认控制域
const quint8 CTRL_FCB_BIT       = 0x20; // 发送侧帧计数位
const quint8 CTRL_ACD_BIT       = 0x20; // 接收侧 ACD：还有后续帧 / 有一级数据

/* ---------- 装置地址（原实现固定为 1） ---------- */
const quint8 DEV_ADDR           = 0x01;

/* ---------- 原硬编码命令帧的控制域 ---------- */
const quint8 CTRL_TIME_SYNC     = 0x67; // 装置对时 10 67 01 66 16
const quint8 CTRL_SIGNAL_RESET  = 0x68; // 信号复归 10 68 01 69 16
// 总召唤原硬编码帧的校验和：按累加规则本应为 0xBB，实际报文中是 0x67，
// 为保证报文逐字节一致，此处按原值写死（既有现象，未修改）。
const quint8 CS_GENERAL_INTERROGATION = 0x67;

/* ---------- ASDU ---------- */
const quint8 ASDU_WAVE_LIST_REQ = 0x0F; // ASDU15 召唤录波列表
const quint8 ASDU_WAVE_LIST_ACK = 0x10; // ASDU16 录波列表应答
const quint8 ASDU_FILE_REQUEST  = 0x0D; // ASDU13 请求录波文件
const quint8 ASDU_FILE_DATA     = 0x0E; // ASDU14 文件分段数据
const quint8 ASDU_GENERAL_INTERROGATION = 0x64; // 总召唤

const quint8 ASDU_VSQ_ONE       = 0x81;
const quint8 ASDU_COT_DEFAULT   = 0x00;
const quint8 ASDU_FUN_DEFAULT   = 0xFF;
const quint8 ASDU_INF_DEFAULT   = 0x00;

/* ---------- COMTRADE 文件包 ---------- */
const quint8 FILE_TYPE_HDR      = 0x01;
const quint8 FILE_TYPE_CFG      = 0x02;
const quint8 FILE_TYPE_DAT      = 0x03;

const int PKG_HEAD_LEN   = int(sizeof(H103ComtradePackageHeader)); // 47
const int BLOCK_HEAD_LEN = int(sizeof(H103FileBlockHeader));       // 5
const int WAVE_ITEM_LEN  = int(sizeof(H103WaveFileItem));          // 48
const int WAVE_HEAD_LEN  = int(sizeof(H103AsduWaveListHead));      // 20
const int VAR_HEAD_LEN   = int(sizeof(H103VariableFrameHeader));   // 6

/* ---------- 保护性上限（与原实现一致） ---------- */
const int     MAX_RECV_BUF        = 2048;              // 接收缓存上限
const quint32 MAX_FILE_PACKAGE    = 10u * 1024u * 1024u; // 10MB

/* ---------- 定时器 ---------- */
const int POLL_INTERVAL_MS        = 10000; // 二级数据轮询周期
const int WAVE_TRANS_TIMEOUT_MS   = 3000;  // 录波列表帧间超时
const int WAVE_LIST_FIRST_TIMEOUT_MS = 5000; // 录波列表首帧超时
const int FILE_DL_FIRST_TIMEOUT_MS   = 1;    // 原实现值（形同立即超时，既有问题，未修改）

// 把 packed 结构体按其内存布局原样写成字节流（sizeof 已由 static_assert 校验）
template <typename T>
inline QByteArray structToBytes(const T &s)
{
    return QByteArray(reinterpret_cast<const char *>(&s), int(sizeof(T)));
}

// 从字节流指定位置读取一个 packed 结构体（memcpy 规避未对齐访问）
template <typename T>
inline bool readStructAt(const QByteArray &data, int pos, T &out)
{
    if (pos < 0 || pos + int(sizeof(T)) > data.size()) {
        return false;
    }
    std::memcpy(&out, data.constData() + pos, sizeof(T));
    return true;
}

// 从文件包中读取一个文件块头的长度；类型不匹配或数据不足返回 false
bool readFileBlockLength(const QByteArray &pkg, int pos, quint8 expectType, quint32 &len)
{
    H103FileBlockHeader block;
    if (!readStructAt(pkg, pos, block)) {
        return false;
    }
    if (block.type != expectType) {
        return false;
    }
    len = block.len; // 小端（Windows/x86）
    return true;
}

} // namespace

/* ============================================================
 * 构造 / 析构
 * ============================================================ */
NorthChina103::NorthChina103(QObject *parent)
    : IProtocol(parent)
    , m_fcb(false)
    , m_pollTimer(new QTimer(this))
    , m_waveTransTimer(nullptr)
    , m_isWaveTransmitting(false)
    , m_isFileDownloading(false)
    , m_fileDownloadTimer(nullptr)
{
    // 二级数据轮询：超时自动组帧并请求发送
    m_pollTimer->setInterval(POLL_INTERVAL_MS);
    connect(m_pollTimer, &QTimer::timeout, this, [=](){
        emit requestSend(buildFrame({{"type", "召唤二级数据"}, {"addr", DEV_ADDR}}));
    });

    // 录波列表多帧传输超时：清空缓存、复位状态、上报错误并恢复轮询
    m_waveTransTimer = new QTimer(this);
    m_waveTransTimer->setSingleShot(true);
    connect(m_waveTransTimer, &QTimer::timeout, this, [=](){
        m_waveListCache.clear();
        m_isWaveTransmitting = false;
        emit waveListError("文件列表传输超时");
        m_pollTimer->start();
    });

    // 录波文件下载超时
    m_fileDownloadTimer = new QTimer(this);
    m_fileDownloadTimer->setSingleShot(true);
    connect(m_fileDownloadTimer, &QTimer::timeout, this, [=](){
        m_filePackageCache.clear();
        m_isFileDownloading = false;
        emit waveDownloadError("录波文件下载超时");
        m_pollTimer->start();
    });
}

NorthChina103::~NorthChina103()
{
}

/* ============================================================
 * 轮询控制
 * ============================================================ */
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

/* ============================================================
 * CP56Time2a 编解码
 * ============================================================ */
H103Cp56Time2a NorthChina103::cp56FromDateTime(const QDateTime &dt)
{
    H103Cp56Time2a t;
    const quint16 ms = quint16(dt.time().msec() + dt.time().second() * 1000);

    t.msL    = quint8(ms & 0xFF);            // 毫秒低 8 位
    t.msH    = quint8((ms >> 8) & 0xFF);     // 毫秒高 8 位
    t.minute = quint8(dt.time().minute() & 0x3F); // IV/RES 位默认 0
    t.hour   = quint8(dt.time().hour() & 0x1F);   // SU/RES 位默认 0

    // 高 3 位星期，低 5 位日期（Qt: 1=周一 ~ 7=周日）
    const int weekDay = dt.date().dayOfWeek();
    t.dayAndWeek = quint8((dt.date().day() & 0x1F) | ((weekDay << 5) & 0xE0));

    t.month = quint8(dt.date().month() & 0x0F);
    t.year  = quint8((dt.date().year() % 100) & 0x7F);
    return t;
}

QDateTime NorthChina103::cp56ToDateTime(const H103Cp56Time2a &t)
{
    const quint16 ms = quint16(t.msL) | (quint16(t.msH) << 8);
    const int minute = t.minute & 0x3F;
    const int hour   = t.hour & 0x1F;
    const int day    = t.dayAndWeek & 0x1F;
    const int month  = t.month & 0x0F;
    const int year   = 2000 + (t.year & 0x7F);

    return QDateTime(QDate(year, month, day), QTime(hour, minute, ms / 1000, ms % 1000));
}

QString NorthChina103::protocolName() const
{
    return "华北电网103 TCP";
}

/* ============================================================
 * 帧组装
 * ============================================================ */
quint8 NorthChina103::takeCtrlWithFcb(quint8 baseCtrl)
{
    quint8 ctrl = baseCtrl;
    if (m_fcb) {
        ctrl |= CTRL_FCB_BIT;
    }
    m_fcb = !m_fcb;
    return ctrl;
}

QByteArray NorthChina103::buildFixedFrame(uchar ctrl, uchar addr)
{
    H103FixedFrame f;
    f.start = H103_START_FIXED;
    f.ctrl  = quint8(ctrl);
    f.addr  = quint8(addr);
    f.cs    = quint8(quint8(ctrl) ^ quint8(addr)); // 固定帧校验和 = 控制域 ^ 地址域
    f.end   = H103_END;
    return structToBytes(f);
}

QByteArray NorthChina103::buildFixedCommandFrame(quint8 ctrl)
{
    // 原实现中“装置对时/信号复归”把地址写死为 1，这里保持一致
    return buildFixedFrame(ctrl, DEV_ADDR);
}

QByteArray NorthChina103::assembleVariableFrame(const QByteArray &asduBody, quint8 addr, quint8 ctrl)
{
    const quint16 len = quint16(2 + asduBody.size()); // 控制域1 + 地址域1 + ASDU

    H103VariableFrameHeader h;
    h.start1 = H103_START_VAR;
    h.lenL   = quint8(len & 0xFF);
    h.lenH   = quint8((len >> 8) & 0xFF);
    h.start2 = H103_START_VAR;
    h.ctrl   = ctrl;
    h.addr   = addr;

    QByteArray frame = structToBytes(h);
    frame.append(asduBody);

    // 帧校验和：从控制域（帧内偏移 4）起累加 len 个字节
    quint8 cs = 0;
    for (int i = VAR_HEAD_LEN - 2; i < frame.size(); ++i) {
        cs = quint8(cs + quint8(frame.at(i)));
    }
    frame.append(char(cs));
    frame.append(char(H103_END));
    return frame;
}

H103AsduWaveListRequest NorthChina103::makeWaveListRequestAsdu(const QDateTime &startTime,
                                                               const QDateTime &endTime)
{
    H103AsduWaveListRequest asdu;
    std::memset(&asdu, 0, sizeof(asdu));

    asdu.hdr.typ      = ASDU_WAVE_LIST_REQ;
    asdu.hdr.vsq      = ASDU_VSQ_ONE;
    asdu.hdr.cot      = ASDU_COT_DEFAULT;
    asdu.hdr.asduAddr = DEV_ADDR;
    asdu.hdr.fun      = ASDU_FUN_DEFAULT;
    asdu.hdr.inf      = ASDU_INF_DEFAULT;

    asdu.startTime = cp56FromDateTime(startTime);
    asdu.endTime   = cp56FromDateTime(endTime);
    return asdu;
}

H103AsduFileRequest NorthChina103::makeFileRequestAsdu(const QString &fileName, quint32 startPos) const
{
    H103AsduFileRequest asdu;
    std::memset(&asdu, 0, sizeof(asdu)); // 文件名不足 40 字节时补 0

    asdu.hdr.typ      = ASDU_FILE_REQUEST;
    asdu.hdr.vsq      = ASDU_VSQ_ONE;
    asdu.hdr.cot      = ASDU_COT_DEFAULT;
    asdu.hdr.asduAddr = DEV_ADDR;
    asdu.hdr.fun      = ASDU_FUN_DEFAULT;
    asdu.hdr.inf      = ASDU_INF_DEFAULT;

    // 文件名固定 40 字节：超长截断、不足补 0（与原实现一致）
    QByteArray nameBytes = fileName.toLatin1();
    if (nameBytes.size() > 40) {
        nameBytes = nameBytes.left(40);
    }
    std::memcpy(asdu.name, nameBytes.constData(), size_t(nameBytes.size()));

    asdu.offset = startPos; // 小端（Windows/x86）
    return asdu;
}

QByteArray NorthChina103::buildGeneralInterrogationFrame()
{
    H103AsduGeneralInterrogation asdu;
    std::memset(&asdu, 0, sizeof(asdu));
    asdu.hdr.typ      = ASDU_GENERAL_INTERROGATION;
    asdu.hdr.vsq      = 0x01;
    asdu.hdr.cot      = 0x01;
    asdu.hdr.asduAddr = DEV_ADDR;
    asdu.hdr.fun      = 0x00;
    asdu.hdr.inf      = 0x00;
    asdu.qoi          = 0x00;

    // 原硬编码报文：长度域为 "09 09"（长度高字节原样为 0x09，与实际帧长不符），
    // 校验和固定 0x67。两处都按原字节保留，不做“修正”。
    H103VariableFrameHeader h;
    h.start1 = H103_START_VAR;
    h.lenL   = 0x09;
    h.lenH   = 0x09;
    h.start2 = H103_START_VAR;
    h.ctrl   = CTRL_VAR_FRAME;
    h.addr   = DEV_ADDR;

    QByteArray frame = structToBytes(h);
    frame.append(structToBytes(asdu));
    frame.append(char(CS_GENERAL_INTERROGATION));
    frame.append(char(H103_END));
    return frame;
}

QByteArray NorthChina103::buildFrame(const QVariantMap &param)
{
    const QString type = param.value("type").toString();
    const quint8 addr = quint8(param.value("addr", int(DEV_ADDR)).toInt());

    // ---------- 固定长度帧 ----------
    if (type == "复位通信单元") {
        return buildFixedFrame(CTRL_RESET_CU, addr);
    }
    if (type == "复位帧计数位") {
        return buildFixedFrame(takeCtrlWithFcb(CTRL_RESET_FCB), addr);
    }
    if (type == "召唤一级数据") {
        return buildFixedFrame(takeCtrlWithFcb(CTRL_SUM_LEVEL1), addr);
    }
    if (type == "召唤二级数据") {
        return buildFixedFrame(takeCtrlWithFcb(CTRL_SUM_LEVEL2), addr);
    }
    if (type == "召唤录波列表") {
        return buildFixedFrame(takeCtrlWithFcb(CTRL_WAVE_LIST), addr);
    }
    if (type == "装置对时") {
        return buildFixedCommandFrame(CTRL_TIME_SYNC);
    }
    if (type == "信号复归") {
        return buildFixedCommandFrame(CTRL_SIGNAL_RESET);
    }

    // ---------- 可变长度帧 ----------
    if (type == "总召唤") {
        return buildGeneralInterrogationFrame();
    }
    if (type == "召唤录波列表_按时间") {
        const QDateTime startTime = param.value("startTime").toDateTime();
        const QDateTime endTime = param.value("endTime").toDateTime();
        const H103AsduWaveListRequest asdu = makeWaveListRequestAsdu(startTime, endTime);
        return assembleVariableFrame(structToBytes(asdu), addr, takeCtrlWithFcb(CTRL_VAR_FRAME));
    }

    return QByteArray();
}

/* ============================================================
 * 录波文件下载：发起
 * ============================================================ */
void NorthChina103::downloadWaveFile(const QString &fileName, quint32 startPos)
{
    // 互斥判断：已有任务在传输则直接返回
    if (m_isFileDownloading) {
        return;
    }

    // 先置全部状态，再发送任何报文
    m_pollTimer->stop();
    m_downloadFileName = fileName;
    m_filePackageCache.clear();
    m_totalFileSize = 0;
    m_hdrLen = 0;
    m_cfgLen = 0;
    m_datLen = 0;
    m_isFileDownloading = true;

    m_fileDownloadTimer->stop();
    m_fileDownloadTimer->start(FILE_DL_FIRST_TIMEOUT_MS); // 保持原值（见头文件注释的既有问题）

    const H103AsduFileRequest asdu = makeFileRequestAsdu(fileName, startPos);
    const QByteArray frame = assembleVariableFrame(structToBytes(asdu), DEV_ADDR,
                                                   takeCtrlWithFcb(CTRL_VAR_FRAME));
    if (frame.isEmpty()) {
        m_pollTimer->start();
        m_fileDownloadTimer->stop();
        emit waveDownloadError("下载召唤帧组帧失败");
        return;
    }

    emit requestSend(frame);

    // 保持原行为：发送后立即停掉下载定时器并恢复二级轮询
    m_fileDownloadTimer->stop();
    m_pollTimer->start();
}

/* ============================================================
 * 拆帧（字节流层面，无法用结构体，保持原实现）
 * ============================================================ */
QList<QByteArray> NorthChina103::splitRawFrame(const QByteArray &data)
{
    int dummy = 0;
    return splitRawFrameWithCache(data, dummy);
}

QList<QByteArray> NorthChina103::splitRawFrameWithCache(const QByteArray &data, int &usedLen)
{
    usedLen = 0;
    QList<QByteArray> frames;
    const int total = data.size();
    int pos = 0;

    while (pos < total)
    {
        const quint8 b = quint8(data.at(pos));

        // 固定帧 0x10 开头
        if (b == H103_START_FIXED) {
            if (pos + 5 <= total && quint8(data.at(pos + 4)) == H103_END) {
                frames.append(data.mid(pos, 5));
                pos += 5;
                continue;
            }
            break;
        }

        // 可变帧 0x68 开头
        if (b == H103_START_VAR) {
            if (pos + 4 > total) break;
            if (quint8(data.at(pos + 3)) != H103_START_VAR) {
                pos++;
                continue;
            }

            const quint8 LL = quint8(data.at(pos + 1));
            const quint8 HH = quint8(data.at(pos + 2));
            const quint16 bodyLen = quint16((HH << 8) | LL);
            const int frameLen = 4 + int(bodyLen) + 2; // 帧头 + 体 + 校验 + 结束符

            if (pos + frameLen > total) break;
            if (quint8(data.at(pos + frameLen - 1)) == H103_END) {
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

/* ============================================================
 * 数据入口：缓存 → 拆帧 → 逐帧分派 → ACD 补发
 * ============================================================ */
void NorthChina103::feedRawData(const QByteArray &data)
{
    m_recvBuf.append(data);

    // 缓存溢出保护（与原实现一致：直接清空）
    if (m_recvBuf.size() > MAX_RECV_BUF) {
        m_recvBuf.clear();
        return;
    }

    int usedLen = 0;
    const QList<QByteArray> frames = splitRawFrameWithCache(m_recvBuf, usedLen);
    m_recvBuf = m_recvBuf.mid(usedLen);

    bool hasAcd = false;
    bool keepGoing = true;
    for (int i = 0; i < frames.size() && keepGoing; ++i) {
        const QByteArray &frame = frames.at(i);
        if (frameAcdSet(frame)) {
            hasAcd = true;
        }
        keepGoing = processFrame(frame); // false 等价于原实现的 break
    }

    // 整批处理完统一响应一级召唤（原实现即使中途 break 也会执行）
    if (hasAcd) {
        requestLevel1Data();
    }
}

bool NorthChina103::frameAcdSet(const QByteArray &frame) const
{
    if (frame.isEmpty()) {
        return false;
    }
    if (frame.size() == 5 && quint8(frame.at(0)) == H103_START_FIXED) {
        return (quint8(frame.at(1)) & CTRL_ACD_BIT) != 0;
    }
    if (quint8(frame.at(0)) == H103_START_VAR && frame.size() >= 5) {
        return (quint8(frame.at(4)) & CTRL_ACD_BIT) != 0;
    }
    return false;
}

bool NorthChina103::processFrame(const QByteArray &frame)
{
    // 固定帧不承载 ASDU 业务，只参与上面的 ACD 判断
    if (frame.isEmpty() || quint8(frame.at(0)) != H103_START_VAR) {
        return true;
    }
    if (frame.size() < 7) {
        return true;
    }
    // 以下长度/校验判断保持原实现的“中断本批帧”语义
    if (frame.size() < 8) {
        return false;
    }

    const quint16 len = quint16(quint8(frame.at(1)) | (quint8(frame.at(2)) << 8));
    if (frame.size() < int(len) + 6) {
        return false;
    }

    quint8 calcCs = 0;
    for (int i = 0; i < int(len); ++i) {
        calcCs = quint8(calcCs + quint8(frame.at(VAR_HEAD_LEN - 2 + i)));
    }
    const int csPos = VAR_HEAD_LEN - 2 + int(len);
    if (quint8(frame.at(csPos)) != calcCs) {
        return true; // 校验失败：丢弃该帧，继续处理后续帧
    }
    if (quint8(frame.at(csPos + 1)) != H103_END) {
        return true;
    }

    const quint8 asduType = quint8(frame.at(VAR_HEAD_LEN));
    switch (asduType) {
    case ASDU_WAVE_LIST_ACK:
        handleWaveListFrame(frame);
        return true;
    case ASDU_FILE_DATA:
        return handleFileDataFrame(frame);
    default:
        return true;
    }
}

/* ============================================================
 * ASDU16：录波文件列表
 * ============================================================ */
void NorthChina103::handleWaveListFrame(const QByteArray &frame)
{
    m_waveListCache.append(parseWaveListAsdu(frame));

    const bool hasMoreFrame = (quint8(frame.at(4)) & CTRL_ACD_BIT) != 0;
    if (!hasMoreFrame) {
        // 最后一帧：发出完整列表信号，清空状态并恢复轮询
        emit waveListReceived(m_waveListCache);
        m_waveListCache.clear();
        m_isWaveTransmitting = false;
        m_waveTransTimer->stop();
        m_pollTimer->start();
    } else {
        m_waveTransTimer->start(WAVE_TRANS_TIMEOUT_MS);
    }
}

QList<WaveFileInfo> NorthChina103::parseWaveListAsdu(const QByteArray &frame)
{
    QList<WaveFileInfo> result;

    // 最小帧长度校验：链路层6 + ASDU头20 + 至少1个条目48 = 74
    if (frame.size() < VAR_HEAD_LEN + WAVE_HEAD_LEN + WAVE_ITEM_LEN) {
        return result;
    }

    // 本帧文件数：ASDU 偏移 4~5 字节，小端序（结构体 H103AsduWaveListHead）
    H103AsduWaveListHead head;
    if (!readStructAt(frame, VAR_HEAD_LEN, head)) {
        return result;
    }
    const quint16 fileCount = head.fileCount;

    int pos = VAR_HEAD_LEN + WAVE_HEAD_LEN;
    for (quint16 i = 0; i < fileCount; ++i) {
        if (pos + WAVE_ITEM_LEN > frame.size()) {
            break;
        }

        H103WaveFileItem item;
        readStructAt(frame, pos, item);

        WaveFileInfo info;
        info.deviceAddr = item.deviceAddr;
        // 文件名固定 40 字节，去掉尾部补 0
        info.fileName = QString::fromLatin1(item.name, 40).remove(QChar('\0'));
        info.faultTime = cp56ToDateTime(item.faultTime);
        info.fileSize = 0;
        result.append(info);

        pos += WAVE_ITEM_LEN;
    }
    return result;
}

/* ============================================================
 * ASDU14：文件分段下载
 * ============================================================ */
bool NorthChina103::handleFileDataFrame(const QByteArray &frame)
{
    if (!m_isFileDownloading) {
        return false;
    }

    H103AsduFileDataHeader asduHdr;
    if (!readStructAt(frame, VAR_HEAD_LEN, asduHdr)) {
        return false;
    }
    const quint32 offset = asduHdr.offset; // 本段起始位置，小端

    // 偏移超过 10MB 直接丢弃，防止越界
    if (offset > MAX_FILE_PACKAGE) {
        return false;
    }

    // 本段数据：链路层6 + ASDU头8 之后，到校验和/结束符之前
    const int dataStart = VAR_HEAD_LEN + int(sizeof(H103AsduFileDataHeader));
    const int dataLen = frame.size() - 2 - dataStart;
    if (dataLen <= 0) {
        return false;
    }

    // 按偏移写入总缓存（支持断点续传、乱序补全）
    const quint32 needSize = offset + quint32(dataLen);
    if (needSize > MAX_FILE_PACKAGE) {
        return false;
    }
    if (needSize > quint32(m_filePackageCache.size())) {
        m_filePackageCache.resize(int(needSize));
    }
    std::memcpy(m_filePackageCache.data() + offset, frame.constData() + dataStart, size_t(dataLen));

    refreshBlockLengths();
    emit waveDownloadProgress(quint32(calcDownloadProgress(m_filePackageCache.size())), 100);

    // 判断 ACD 是否还有后续帧
    const bool hasMore = (quint8(frame.at(4)) & CTRL_ACD_BIT) != 0;
    if (!hasMore) {
        finishFileDownload();
    }
    return true;
}

/* ============================================================
 * COMTRADE 文件包：长度解析 / 进度 / 收尾（全文件唯一一处）
 * ============================================================ */
void NorthChina103::refreshBlockLengths()
{
    quint32 len = 0;

    // HDR 块长度：紧跟在 47 字节公共头之后
    if (m_hdrLen == 0
        && readFileBlockLength(m_filePackageCache, PKG_HEAD_LEN, FILE_TYPE_HDR, len)) {
        m_hdrLen = len;
    }

    // CFG 块长度：HDR 内容之后
    if (m_cfgLen == 0 && m_hdrLen > 0) {
        const int cfgPos = PKG_HEAD_LEN + BLOCK_HEAD_LEN + int(m_hdrLen);
        if (readFileBlockLength(m_filePackageCache, cfgPos, FILE_TYPE_CFG, len)) {
            m_cfgLen = len;
        }
    }

    // DAT 块长度：CFG 内容之后
    if (m_datLen == 0 && m_cfgLen > 0) {
        const int datPos = PKG_HEAD_LEN + 2 * BLOCK_HEAD_LEN + int(m_hdrLen) + int(m_cfgLen);
        if (readFileBlockLength(m_filePackageCache, datPos, FILE_TYPE_DAT, len)) {
            m_datLen = len;
        }
    }
}

int NorthChina103::calcDownloadProgress(qint64 curPos) const
{
    // 三文件等权重：HDR 33.33% + CFG 33.33% + DAT 33.34%（与原公式一致）
    double totalPercent = 0.0;

    if (curPos <= PKG_HEAD_LEN) {
        totalPercent = 0.0;
    } else if (m_hdrLen > 0 && curPos <= qint64(PKG_HEAD_LEN + BLOCK_HEAD_LEN) + qint64(m_hdrLen)) {
        const qint64 innerPos = curPos - (PKG_HEAD_LEN + BLOCK_HEAD_LEN);
        const double innerPercent = qMin(1.0, double(innerPos) / double(m_hdrLen));
        totalPercent = innerPercent * 33.33;
    } else if (m_cfgLen > 0
               && curPos <= qint64(PKG_HEAD_LEN + 2 * BLOCK_HEAD_LEN) + qint64(m_hdrLen) + qint64(m_cfgLen)) {
        const qint64 cfgStart = PKG_HEAD_LEN + BLOCK_HEAD_LEN + qint64(m_hdrLen) + BLOCK_HEAD_LEN;
        const qint64 innerPos = curPos - cfgStart;
        const double innerPercent = qMin(1.0, double(innerPos) / double(m_cfgLen));
        totalPercent = 33.33 + innerPercent * 33.33;
    } else if (m_datLen > 0) {
        const qint64 datStart = PKG_HEAD_LEN + 2 * BLOCK_HEAD_LEN
                                + qint64(m_hdrLen) + qint64(m_cfgLen) + BLOCK_HEAD_LEN;
        const qint64 innerPos = curPos - datStart;
        const double innerPercent = qMin(1.0, double(innerPos) / double(m_datLen));
        totalPercent = 66.66 + innerPercent * 33.34;
    } else {
        // 跨阶段且未解析到下一个文件长度时，平滑过渡
        totalPercent = 33.33;
    }

    return qRound(totalPercent);
}

void NorthChina103::finishFileDownload()
{
    QByteArray hdr, cfg, dat;
    const bool parseOk = parseComtradePackage(m_filePackageCache, hdr, cfg, dat);

    m_isFileDownloading = false;
    m_pollTimer->start(); // 恢复二级轮询

    const quint32 total = quint32(m_filePackageCache.size());
    emit waveDownloadProgress(total, total);

    if (parseOk) {
        emit waveDownloadFinished(m_downloadFileName, hdr, cfg, dat);
    } else {
        emit waveDownloadError("feedRawData-文件包解析失败，格式不符合规约");
    }
}

bool NorthChina103::parseComtradePackage(const QByteArray &pkg, QByteArray &outHdr,
                                         QByteArray &outCfg, QByteArray &outDat)
{
    if (pkg.size() < PKG_HEAD_LEN) {
        return false;
    }

    // 跳过 47 字节公共头（40 文件名 + 7 故障时间）
    int pos = PKG_HEAD_LEN;

    // 依次解析 3 个文件块（类型 1=HDR 2=CFG 3=DAT）
    for (int i = 0; i < 3; ++i) {
        H103FileBlockHeader block;
        if (!readStructAt(pkg, pos, block)) {
            return false;
        }
        pos += BLOCK_HEAD_LEN;

        if (block.len > quint32(pkg.size() - pos)) {
            return false;
        }
        const QByteArray content = pkg.mid(pos, int(block.len));
        pos += int(block.len);

        switch (block.type) {
        case FILE_TYPE_HDR: outHdr = content; break;
        case FILE_TYPE_CFG: outCfg = content; break;
        case FILE_TYPE_DAT: outDat = content; break;
        default: return false;
        }
    }
    return true;
}

/* ============================================================
 * 对外业务接口
 * ============================================================ */
void NorthChina103::callWaveFileList(const QDateTime &startTime, const QDateTime &endTime)
{
    // 暂停二级数据轮询，避免 FCB 位冲突打断录波传输
    m_pollTimer->stop();

    // 重置传输状态，清空历史缓存
    m_waveListCache.clear();
    m_isWaveTransmitting = true;
    m_waveTransTimer->start(WAVE_LIST_FIRST_TIMEOUT_MS); // 首帧超时 5 秒

    m_tempFileList.clear();

    QVariantMap param;
    param["type"] = "召唤录波列表_按时间";
    param["addr"] = DEV_ADDR; // 原实现传了但未使用（ASDU15 组帧内部固定地址）
    param["startTime"] = startTime;
    param["endTime"] = endTime;

    const QByteArray frame = buildFrame(param);
    if (!frame.isEmpty()) {
        emit requestSend(frame);
    }
}

void NorthChina103::requestLevel1Data()
{
    emit requestSend(buildFrame({{"type", "召唤一级数据"}, {"addr", DEV_ADDR}}));
}

/* ============================================================
 * 报文解析展示（只读，不改发送字节）
 * ============================================================ */
namespace {

QString hexByteText(quint8 v)
{
    return QString("%1").arg(v, 2, 16, QChar('0')).toUpper();
}

QString hexWordText(quint16 v)
{
    return QString("%1").arg(v, 4, 16, QChar('0')).toUpper();
}

// 字节区间转十六进制文本；超过 8 字节只列出前 8 个，避免单行过长
QString hexRangeText(const QByteArray &frame, int pos, int count)
{
    if (pos < 0 || count <= 0 || pos + count > frame.size()) {
        return QString();
    }
    QByteArray part = frame.mid(pos, count);
    if (count > 8) {
        part = part.left(8);
        return part.toHex(' ').toUpper() + QStringLiteral(" …");
    }
    return part.toHex(' ').toUpper();
}

quint32 readU32LE(const QByteArray &data, int pos)
{
    return quint32(quint8(data.at(pos)))
         | (quint32(quint8(data.at(pos + 1))) << 8)
         | (quint32(quint8(data.at(pos + 2))) << 16)
         | (quint32(quint8(data.at(pos + 3))));
}

QString ctrlNameText(quint8 ctrl)
{
    switch (ctrl) {
    case 0x08: return QStringLiteral("召唤录波列表（FCB=0）");
    case 0x28: return QStringLiteral("召唤录波列表（FCB=1）");
    case 0x40: return QStringLiteral("复位通信单元");
    case 0x60: return QStringLiteral("复位通信单元（FCB=1）");
    case 0x47: return QStringLiteral("复位帧计数位（FCB=0）");
    case 0x67: return QStringLiteral("复位帧计数位（FCB=1）/ 装置对时");
    case 0x5A: return QStringLiteral("召唤一级数据（FCB=0）");
    case 0x7A: return QStringLiteral("召唤一级数据（FCB=1）");
    case 0x5B: return QStringLiteral("召唤二级数据（FCB=0）");
    case 0x7B: return QStringLiteral("召唤二级数据（FCB=1）");
    case 0x68: return QStringLiteral("信号复归");
    default:   return QString();
    }
}

QString ctrlText(quint8 ctrl)
{
    QString text = QString("二进制 %1：bit7=%2 bit6=%3 bit5(FCB/ACD)=%4 bit4=%5 功能码=0x%6")
                       .arg(QString::number(ctrl, 2).rightJustified(8, QChar('0')))
                       .arg((ctrl >> 7) & 1)
                       .arg((ctrl >> 6) & 1)
                       .arg((ctrl >> 5) & 1)
                       .arg((ctrl >> 4) & 1)
                       .arg(hexByteText(quint8(ctrl & 0x0F)));
    const QString name = ctrlNameText(ctrl);
    if (!name.isEmpty()) {
        text += QString("（%1）").arg(name);
    }
    return text;
}

QString asduCotName(quint8 cause)
{
    switch (cause) {
    case 0x00: return QStringLiteral("未定义");
    case 0x01: return QStringLiteral("周期/循环");
    case 0x02: return QStringLiteral("背景扫描");
    case 0x03: return QStringLiteral("突发");
    case 0x04: return QStringLiteral("初始化");
    case 0x05: return QStringLiteral("请求或被请求");
    case 0x06: return QStringLiteral("激活");
    case 0x07: return QStringLiteral("激活确认");
    case 0x08: return QStringLiteral("停止激活");
    case 0x09: return QStringLiteral("停止激活确认");
    case 0x0A: return QStringLiteral("激活终止");
    case 0x14: return QStringLiteral("响应站召唤");
    default:   return QStringLiteral("未知传送原因");
    }
}

QString asduTypeName(quint8 typ)
{
    switch (typ) {
    case 0x01: return QStringLiteral("ASDU1 带时标的报文");
    case 0x02: return QStringLiteral("ASDU2 带相对时间的报文");
    case 0x03: return QStringLiteral("ASDU3 被测值 I");
    case 0x04: return QStringLiteral("ASDU4 被测值 II");
    case 0x05: return QStringLiteral("ASDU5 标识");
    case 0x06: return QStringLiteral("ASDU6 时间同步");
    case 0x07: return QStringLiteral("ASDU7 总召唤启动");
    case 0x08: return QStringLiteral("ASDU8 总召唤终止");
    case 0x09: return QStringLiteral("ASDU9 被测值 II 带时标");
    case 0x0A: return QStringLiteral("ASDU10 通用分类数据");
    case 0x0B: return QStringLiteral("ASDU11 通用分类标识");
    case 0x0C: return QStringLiteral("ASDU12 通用分类命令");
    case 0x0D: return QStringLiteral("ASDU13 请求录波文件");
    case 0x0E: return QStringLiteral("ASDU14 文件分段数据");
    case 0x0F: return QStringLiteral("ASDU15 召唤录波文件列表");
    case 0x10: return QStringLiteral("ASDU16 录波文件列表应答");
    case 0x64: return QStringLiteral("总召唤（本项目硬编码帧）");
    default:   return QStringLiteral("未定义的 ASDU 类型");
    }
}

} // namespace

ProtocolParseResult NorthChina103::parseFrame(const QByteArray &frame)
{
    ProtocolParseResult res;
    res.rawHex = frame.toHex(' ').toUpper();

    if (frame.isEmpty()) {
        res.detailDesc = QStringLiteral("报文为空");
        return res;
    }
    if (frame.size() < 5) {
        res.detailDesc = QString("报文长度 %1 字节，不足一帧的最小长度（5 字节）").arg(frame.size());
        return res;
    }

    // 每个字段一行：偏移 | 字节 | 字段名 | 说明
    QStringList detail;
    auto addRow = [&detail](int pos, const QString &bytes, const QString &name, const QString &note) {
        QString line = QString("[%1] %2  %3")
                           .arg(QString::number(pos).rightJustified(2, QChar('0')),
                                bytes.leftJustified(21, QChar(' ')),
                                name);
        if (!note.isEmpty()) {
            line += QString("：") + note;
        }
        detail << line;
    };

    const quint8 startByte = quint8(frame.at(0));
    const quint8 endByte = quint8(frame.at(frame.size() - 1));

    auto cp56Text = [this](const H103Cp56Time2a &t) -> QString {
        const quint16 ms = quint16(t.msL) | (quint16(t.msH) << 8);
        const QDateTime dt = cp56ToDateTime(t);
        if (dt.isValid()) {
            return QString("%1（星期%2，毫秒值 %3）")
                .arg(dt.toString("yyyy-MM-dd hh:mm:ss.zzz"))
                .arg(t.dayAndWeek >> 5)
                .arg(ms);
        }
        return QString("非法时间：分=%1 时=%2 日=%3 星期=%4 月=%5 年=%6 毫秒=%7")
            .arg(t.minute & 0x3F).arg(t.hour & 0x1F).arg(t.dayAndWeek & 0x1F)
            .arg(t.dayAndWeek >> 5).arg(t.month & 0x0F).arg(2000 + (t.year & 0x7F))
            .arg(ms);
    };

    // ================= 固定长度帧：10 | 控制域 | 地址域 | 校验和 | 16 =================
    if (startByte == H103_START_FIXED) {
        if (frame.size() != 5) {
            res.detailDesc = QString("首字节为固定帧启动符 0x10，但帧长 %1 字节，与固定帧结构（5 字节）不符")
                                 .arg(frame.size());
            return res;
        }

        H103FixedFrame f = {};
        if (!readStructAt(frame, 0, f)) {
            res.detailDesc = QStringLiteral("固定帧结构读取失败");
            return res;
        }
        const quint8 calcCs = quint8(f.ctrl ^ f.addr);

        addRow(0, hexByteText(f.start), QStringLiteral("启动符"),
               f.start == H103_START_FIXED ? QStringLiteral("固定长度帧启动符 0x10")
                                            : QStringLiteral("应为 0x10"));
        addRow(1, hexByteText(f.ctrl), QStringLiteral("控制域"), ctrlText(f.ctrl));
        addRow(2, hexByteText(f.addr), QStringLiteral("地址域"),
               QString("装置地址 %1").arg(f.addr));
        addRow(3, hexByteText(f.cs), QStringLiteral("校验和"),
               QString("计算值 = 控制域 ^ 地址域 = %1，%2")
                   .arg(hexByteText(calcCs),
                        f.cs == calcCs ? QStringLiteral("校验正确") : QStringLiteral("校验错误")));
        addRow(4, hexByteText(f.end), QStringLiteral("结束符"),
               f.end == H103_END ? QStringLiteral("固定值 0x16") : QStringLiteral("应为 0x16"));

        res.fieldList << qMakePair(QStringLiteral("启动符"), hexByteText(f.start))
                      << qMakePair(QStringLiteral("控制域"), hexByteText(f.ctrl))
                      << qMakePair(QStringLiteral("地址域"), hexByteText(f.addr))
                      << qMakePair(QStringLiteral("校验和"), hexByteText(f.cs))
                      << qMakePair(QStringLiteral("结束符"), hexByteText(f.end));

        res.ok = (f.start == H103_START_FIXED && f.end == H103_END && f.cs == calcCs);
        res.detailDesc = (res.ok ? QString()
                                 : QStringLiteral("注意：启动符 / 结束符 / 校验和存在异常\n"))
                         + detail.join(QChar('\n'));
        return res;
    }

    // ========== 可变长度帧：68 | 长度低 | 长度高 | 68 | 控制域 | 地址域 | ASDU | 校验和 | 16 ==========
    if (startByte != H103_START_VAR) {
        res.detailDesc = QString("非华北103帧：首字节 %1 既不是固定帧启动符 0x10，也不是可变帧启动符 0x68")
                             .arg(hexByteText(startByte));
        return res;
    }
    if (frame.size() < VAR_HEAD_LEN + 2) {
        res.detailDesc = QString("可变帧长度 %1 字节，不足帧头 6 字节 + 校验 1 字节 + 结束符 1 字节")
                             .arg(frame.size());
        return res;
    }

    H103VariableFrameHeader h = {};
    if (!readStructAt(frame, 0, h)) {
        res.detailDesc = QStringLiteral("可变帧帧头读取失败");
        return res;
    }

    const quint16 lenValue = quint16(h.lenL) | (quint16(h.lenH) << 8);
    const int csPos = frame.size() - 2;
    const quint8 csActual = quint8(frame.at(csPos));
    const bool lenMatch = (int(VAR_HEAD_LEN - 2) + int(lenValue) + 2 == frame.size());
    const int asduStart = VAR_HEAD_LEN;
    const int bodyLen = csPos - asduStart;

    // 校验和：从控制域（偏移 4）起累加“长度值”个字节；长度域异常时按实际字节退化为累加
    quint8 calcCs = 0;
    const int sumBegin = VAR_HEAD_LEN - 2;
    const int sumEnd = qMin(csPos, sumBegin + int(lenValue));
    for (int i = sumBegin; i < sumEnd; ++i) {
        calcCs = quint8(calcCs + quint8(frame.at(i)));
    }
    const bool csOk = (sumEnd == sumBegin + int(lenValue)) && (csActual == calcCs);

    addRow(0, hexByteText(h.start1), QStringLiteral("启动符"),
           QStringLiteral("可变长度帧启动符 0x68"));
    addRow(1, hexByteText(h.lenL), QStringLiteral("长度低字节"),
           QString("长度值低 8 位，长度值 = 0x%1 = %2")
               .arg(hexWordText(lenValue))
               .arg(lenValue));
    addRow(2, hexByteText(h.lenH), QStringLiteral("长度高字节"),
           QStringLiteral("长度值高 8 位；长度值 = 控制域 1 + 地址域 1 + ASDU 长度"));
    addRow(3, hexByteText(h.start2), QStringLiteral("第二启动符"),
           h.start2 == H103_START_VAR ? QStringLiteral("固定值 0x68")
                                      : QStringLiteral("应为 0x68，该字节异常"));
    addRow(4, hexByteText(h.ctrl), QStringLiteral("控制域"), ctrlText(h.ctrl));
    addRow(5, hexByteText(h.addr), QStringLiteral("地址域"),
           QString("装置地址 %1").arg(h.addr));

    res.fieldList << qMakePair(QStringLiteral("启动符"), hexByteText(h.start1))
                  << qMakePair(QStringLiteral("长度"),
                               QString("0x%1(%2字节)").arg(hexWordText(lenValue)).arg(lenValue))
                  << qMakePair(QStringLiteral("控制域"), hexByteText(h.ctrl))
                  << qMakePair(QStringLiteral("地址域"), hexByteText(h.addr));

    // ---------- ASDU ----------
    if (bodyLen > 0) {
        const quint8 typ = quint8(frame.at(asduStart));

        auto addCommonHeader = [&](bool withFunInf) {
            addRow(asduStart, hexByteText(typ), QStringLiteral("类型标识 TYP"), asduTypeName(typ));

            const quint8 vsq = quint8(frame.at(asduStart + 1));
            addRow(asduStart + 1, hexByteText(vsq), QStringLiteral("可变结构限定词 VSQ"),
                   QString("SQ=%1，信息数目=%2").arg((vsq >> 7) & 1).arg(vsq & 0x7F));

            const quint8 cot = quint8(frame.at(asduStart + 2));
            addRow(asduStart + 2, hexByteText(cot), QStringLiteral("传送原因 COT"),
                   QString("P/N=%1，原因 0x%2（%3）")
                       .arg((cot >> 6) & 1)
                       .arg(hexByteText(quint8(cot & 0x3F)))
                       .arg(asduCotName(quint8(cot & 0x3F))));

            addRow(asduStart + 3, hexByteText(quint8(frame.at(asduStart + 3))),
                   QStringLiteral("ASDU 公共地址"), QStringLiteral("装置地址"));

            if (withFunInf) {
                addRow(asduStart + 4, hexByteText(quint8(frame.at(asduStart + 4))),
                       QStringLiteral("功能码 FUN"), QStringLiteral("本项目固定填 0xFF"));
                addRow(asduStart + 5, hexByteText(quint8(frame.at(asduStart + 5))),
                       QStringLiteral("信息序号 INF"), QStringLiteral("本项目固定填 0x00"));
            }

            res.fieldList << qMakePair(QStringLiteral("TYP"),
                                       QString("%1（%2）").arg(hexByteText(typ), asduTypeName(typ)));
        };

        if (typ == ASDU_FILE_DATA) {
            // ASDU14 只有 4 字节公共前缀 + 4 字节偏移
            if (bodyLen < 8) {
                detail << QString("ASDU14 数据长度 %1 字节，不足 8 字节（4 字节前缀 + 4 字节偏移）").arg(bodyLen);
            } else {
                addRow(asduStart, hexByteText(typ), QStringLiteral("类型标识 TYP"), asduTypeName(typ));
                const quint8 vsq = quint8(frame.at(asduStart + 1));
                addRow(asduStart + 1, hexByteText(vsq), QStringLiteral("可变结构限定词 VSQ"),
                       QString("SQ=%1，信息数目=%2").arg((vsq >> 7) & 1).arg(vsq & 0x7F));
                const quint8 cot = quint8(frame.at(asduStart + 2));
                addRow(asduStart + 2, hexByteText(cot), QStringLiteral("传送原因 COT"),
                       QString("P/N=%1，原因 0x%2（%3）")
                           .arg((cot >> 6) & 1)
                           .arg(hexByteText(quint8(cot & 0x3F)))
                           .arg(asduCotName(quint8(cot & 0x3F))));
                addRow(asduStart + 3, hexByteText(quint8(frame.at(asduStart + 3))),
                       QStringLiteral("ASDU 公共地址"), QStringLiteral("装置地址"));

                const quint32 offset = readU32LE(frame, asduStart + 4);
                addRow(asduStart + 4, hexRangeText(frame, asduStart + 4, 4),
                       QStringLiteral("本段起始偏移（4 字节小端）"),
                       QString("偏移 %1，本段为文件从该位置起的数据").arg(offset));

                res.fieldList << qMakePair(QStringLiteral("TYP"),
                                           QString("%1（%2）").arg(hexByteText(typ), asduTypeName(typ)))
                              << qMakePair(QStringLiteral("本段偏移"), QString::number(offset));
            }
        } else if (typ == ASDU_WAVE_LIST_ACK) {
            // ASDU16：20 字节自定义头 + 若干个 48 字节条目
            if (bodyLen < int(sizeof(H103AsduWaveListHead))) {
                detail << QString("ASDU16 数据长度 %1 字节，不足 20 字节头部").arg(bodyLen);
            } else {
                H103AsduWaveListHead head = {};
                readStructAt(frame, asduStart, head);

                addRow(asduStart, hexByteText(head.typ), QStringLiteral("类型标识 TYP"), asduTypeName(head.typ));
                addRow(asduStart + 1, hexByteText(head.vsq), QStringLiteral("可变结构限定词 VSQ"),
                       QString("SQ=%1，信息数目=%2").arg((head.vsq >> 7) & 1).arg(head.vsq & 0x7F));
                addRow(asduStart + 2, hexByteText(head.cot), QStringLiteral("传送原因 COT"),
                       QString("P/N=%1，原因 0x%2（%3）")
                           .arg((head.cot >> 6) & 1)
                           .arg(hexByteText(quint8(head.cot & 0x3F)))
                           .arg(asduCotName(quint8(head.cot & 0x3F))));
                addRow(asduStart + 3, hexByteText(head.asduAddr), QStringLiteral("ASDU 公共地址"),
                       QStringLiteral("装置地址"));
                addRow(asduStart + 4, hexRangeText(frame, asduStart + 4, 2),
                       QStringLiteral("本帧文件数（2 字节小端）"), QString::number(head.fileCount));
                addRow(asduStart + 6, hexRangeText(frame, asduStart + 6, 14),
                       QStringLiteral("保留（14 字节）"), QStringLiteral("本程序未使用，保持原偏移"));

                res.fieldList << qMakePair(QStringLiteral("TYP"),
                                           QString("%1（%2）").arg(hexByteText(head.typ), asduTypeName(head.typ)))
                              << qMakePair(QStringLiteral("本帧文件数"), QString::number(head.fileCount));

                int itemPos = asduStart + int(sizeof(H103AsduWaveListHead));
                for (quint16 i = 0; i < head.fileCount; ++i) {
                    if (itemPos + int(sizeof(H103WaveFileItem)) > csPos) {
                        detail << QString("…… 第 %1 个文件条目数据不足（需要 48 字节），后续未展开").arg(i + 1);
                        break;
                    }
                    H103WaveFileItem item = {};
                    readStructAt(frame, itemPos, item);

                    QByteArray nameBytes = QByteArray(reinterpret_cast<const char *>(item.name), 40);
                    const int zero = nameBytes.indexOf('\0');
                    if (zero >= 0) {
                        nameBytes.truncate(zero);
                    }

                    detail << QString("第 %1 个文件条目（偏移 %2，共 48 字节）：").arg(i + 1).arg(itemPos);
                    addRow(itemPos, hexByteText(item.deviceAddr), QStringLiteral("装置地址"),
                           QString::number(item.deviceAddr));
                    addRow(itemPos + 1, hexRangeText(frame, itemPos + 1, 40),
                           QStringLiteral("文件名（40 字节）"), QString::fromLatin1(nameBytes));
                    addRow(itemPos + 41, hexRangeText(frame, itemPos + 41, 7),
                           QStringLiteral("故障时间 CP56Time2a"), cp56Text(item.faultTime));
                    itemPos += int(sizeof(H103WaveFileItem));
                }
            }
        } else {
            // ASDU15 / ASDU13 / 总召唤 / 其它带 6 字节公共头的类型
            if (bodyLen < 6) {
                detail << QString("ASDU 数据长度 %1 字节，不足 6 字节公共头").arg(bodyLen);
            } else {
                addCommonHeader(true);

                if (typ == ASDU_WAVE_LIST_REQ) {
                    if (bodyLen < 20) {
                        detail << QString("ASDU15 数据长度 %1 字节，不足 20 字节").arg(bodyLen);
                    } else {
                        H103Cp56Time2a t1 = {};
                        H103Cp56Time2a t2 = {};
                        readStructAt(frame, asduStart + 6, t1);
                        readStructAt(frame, asduStart + 13, t2);

                        addRow(asduStart + 6, hexRangeText(frame, asduStart + 6, 7),
                               QStringLiteral("起始时间 CP56Time2a"), cp56Text(t1));
                        addRow(asduStart + 13, hexRangeText(frame, asduStart + 13, 7),
                               QStringLiteral("结束时间 CP56Time2a"), cp56Text(t2));

                        res.fieldList << qMakePair(QStringLiteral("起始时间"), cp56Text(t1))
                                      << qMakePair(QStringLiteral("结束时间"), cp56Text(t2));
                    }
                } else if (typ == ASDU_FILE_REQUEST) {
                    if (bodyLen < 50) {
                        detail << QString("ASDU13 数据长度 %1 字节，不足 50 字节").arg(bodyLen);
                    } else {
                        QByteArray nameBytes = frame.mid(asduStart + 6, 40);
                        const int zero = nameBytes.indexOf('\0');
                        if (zero >= 0) {
                            nameBytes.truncate(zero);
                        }
                        const quint32 offset = readU32LE(frame, asduStart + 46);

                        addRow(asduStart + 6, hexRangeText(frame, asduStart + 6, 40),
                               QStringLiteral("文件名（40 字节）"), QString::fromLatin1(nameBytes));
                        addRow(asduStart + 46, hexRangeText(frame, asduStart + 46, 4),
                               QStringLiteral("请求起始偏移（4 字节小端）"), QString::number(offset));

                        res.fieldList << qMakePair(QStringLiteral("文件名"), QString::fromLatin1(nameBytes))
                                      << qMakePair(QStringLiteral("起始偏移"), QString::number(offset));
                    }
                } else if (typ == ASDU_GENERAL_INTERROGATION) {
                    if (bodyLen < 7) {
                        detail << QString("总召唤数据长度 %1 字节，缺少 QOI").arg(bodyLen);
                    } else {
                        const quint8 qoi = quint8(frame.at(asduStart + 6));
                        addRow(asduStart + 6, hexByteText(qoi), QStringLiteral("QOI 限定词"),
                               QString("总召唤限定词 0x%1").arg(hexByteText(qoi)));
                        res.fieldList << qMakePair(QStringLiteral("QOI"), hexByteText(qoi));
                    }
                } else if (bodyLen > 6) {
                    addRow(asduStart + 6, hexRangeText(frame, asduStart + 6, bodyLen - 6),
                           QStringLiteral("ASDU 数据"), QStringLiteral("该类型未做字段展开"));
                }
            }
        }
    } else {
        detail << QStringLiteral("长度域/帧长异常：报文在帧头之后没有 ASDU 数据");
    }

    // ---------- 校验和与结束符 ----------
    QString csNote;
    if (sumEnd != sumBegin + int(lenValue)) {
        csNote = QString("长度域异常，无法按长度域校验；按实际字节（偏移 %1~%2）累加得 0x%3，报文校验和为 0x%4")
                     .arg(sumBegin).arg(sumEnd - 1)
                     .arg(hexByteText(calcCs), hexByteText(csActual));
    } else {
        csNote = QString("从偏移 %1（控制域起）累加 %2 个字节得 0x%3，%4")
                     .arg(sumBegin).arg(lenValue).arg(hexByteText(calcCs),
                         csActual == calcCs ? QStringLiteral("校验正确") : QStringLiteral("校验错误"));
    }
    addRow(csPos, hexByteText(csActual), QStringLiteral("校验和"), csNote);
    addRow(csPos + 1, hexByteText(endByte), QStringLiteral("结束符"),
           endByte == H103_END ? QStringLiteral("固定值 0x16") : QStringLiteral("应为 0x16"));

    res.fieldList << qMakePair(QStringLiteral("校验和"), hexByteText(csActual))
                  << qMakePair(QStringLiteral("结束符"), hexByteText(endByte));

    if (!lenMatch) {
        detail << QString("注意：长度域值 %1（0x%2）与实际帧长不符：按长度域应为 %3 字节，实际 %4 字节%5")
                      .arg(lenValue).arg(hexWordText(lenValue))
                      .arg(int(VAR_HEAD_LEN - 2) + int(lenValue) + 2).arg(frame.size())
                      .arg(quint8(frame.at(asduStart)) == ASDU_GENERAL_INTERROGATION
                               ? QStringLiteral("；本项目总召唤报文长度域硬编码为 09 09，属既有现象")
                               : QString());
    }

    res.ok = (h.start2 == H103_START_VAR && endByte == H103_END && lenMatch && csOk);
    if (!res.ok) {
        detail.prepend(QStringLiteral("注意：报文结构 / 校验存在异常（见下方标注）"));
    }
    res.detailDesc = detail.join(QChar('\n'));
    return res;
}

/* ============================================================
 * 缓存接口
 * ============================================================ */
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
