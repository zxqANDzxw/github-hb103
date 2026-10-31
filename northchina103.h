#ifndef NORTHCHINA103_H
#define NORTHCHINA103_H

#include "iprotocol.h"
#include "h103frames.h"
#include <QObject>
#include <QByteArray>
#include <QString>
#include <QList>
#include <QDateTime>
#include <QTimer>


class NorthChina103 : public IProtocol
{
    Q_OBJECT
public:
    explicit NorthChina103(QObject *parent = nullptr);
    ~NorthChina103() override;

    // ========== 基类标准接口 ==========
    QString protocolName() const override;
    QList<QByteArray> splitRawFrame(const QByteArray& recvBytes) override;
    QList<QByteArray> splitRawFrameWithCache(const QByteArray &data, int &usedLen) override;
    void feedRawData(const QByteArray &data) override;
    QByteArray getRecvBuf() const override;
    void setRecvBuf(const QByteArray &buf) override;
    void clearRecvBuf() override;
    void startPoll() override;
    void stopPoll() override;
    ProtocolParseResult parseFrame(const QByteArray& frame) override;
    QByteArray buildFrame(const QVariantMap& param) override;

    void downloadWaveFile(const QString &fileName,quint32 startPos = 0) override;
    // ========== 对外业务接口 ==========
    // 按时间范围召唤录波文件列表
    void callWaveFileList(const QDateTime& startTime, const QDateTime& endTime)override;

private:
    // ========== 帧组装（结构体编码） ==========
    // 固定帧：10|ctrl|addr|ctrl^addr|16
    QByteArray buildFixedFrame(uchar ctrl, uchar addr);
    // 固定地址(=0x01)的固定帧，用于原实现里写死地址的“装置对时/信号复归”
    QByteArray buildFixedCommandFrame(quint8 ctrl);
    // 总召唤：保持原硬编码报文的全部字节（含固定 CS=0x67）
    QByteArray buildGeneralInterrogationFrame();
    // 可变帧：68|len|68|ctrl|addr|ASDU|CS|16，CS = 控制域起 len 字节累加
    QByteArray assembleVariableFrame(const QByteArray &asduBody, quint8 addr, quint8 ctrl);
    // 按 FCB 翻转位生成控制域（每调用一次翻转一次，与原 m_fcb 行为一致）
    quint8 takeCtrlWithFcb(quint8 baseCtrl);
    // ASDU15：按时间范围召唤录波列表
    H103AsduWaveListRequest makeWaveListRequestAsdu(const QDateTime& startTime,
                                                    const QDateTime& endTime);
    // ASDU13：请求下载录波文件
    H103AsduFileRequest makeFileRequestAsdu(const QString& fileName, quint32 startPos) const;

    // ========== CP56Time2a 编解码（唯一一套，全文件复用） ==========
    H103Cp56Time2a cp56FromDateTime(const QDateTime& dt);
    QDateTime cp56ToDateTime(const H103Cp56Time2a& bytes);

    // ========== 接收解析（按职责分派） ==========
    // 单帧入口；返回 false 等价于原实现里的 break（中断本批剩余帧）
    bool processFrame(const QByteArray &frame);
    // 判断帧是否置位 ACD（接收侧“还有后续帧/有一级数据”标志）
    bool frameAcdSet(const QByteArray &frame) const;
    // ASDU16：录波文件列表（含多帧累积）
    void handleWaveListFrame(const QByteArray &frame);
    // ASDU14：文件分段数据；返回 false 等价于原实现的 break
    bool handleFileDataFrame(const QByteArray &frame);
    // ASDU16：解析单帧录波列表条目
    QList<WaveFileInfo> parseWaveListAsdu(const QByteArray& frame);
    // 请求一级用户数据（多帧续传用）
    void requestLevel1Data();

    // ========== COMTRADE 文件包（唯一一处长度解析） ==========
    // 根据已累积数据刷新 HDR/CFG/DAT 三段长度（未到齐时保持为 0）
    void refreshBlockLengths();
    // 按三段等权重计算下载百分比（与原实现公式一致）
    int calcDownloadProgress(qint64 curPos) const;
    // 传输结束：拆分文件、上报信号、复位状态
    void finishFileDownload();
    // 解析完整 COMTRADE 文件包
    bool parseComtradePackage(const QByteArray &pkg,
                              QByteArray &outHdr,
                              QByteArray &outCfg,
                              QByteArray &outDat);

    // ========== 内部成员变量 ==========
    bool m_fcb;                  // 帧计数位
    QByteArray m_recvBuf;        // 跨包接收缓存
    QTimer *m_pollTimer;         // 二级数据轮询定时器
    QList<WaveFileInfo> m_tempFileList; // 录波列表多包累计缓存（历史遗留，未使用）

    // 录波文件列表多帧传输
    QList<WaveFileInfo> m_waveListCache;   // 多帧临时缓存
    QTimer *m_waveTransTimer;              // 传输超时定时器
    bool m_isWaveTransmitting;             // 传输状态标记

    // ========== 录波文件下载 ==========
    QString m_downloadFileName;    // 当前下载的文件名
    QByteArray m_filePackageCache; // 完整文件包缓存
    bool m_isFileDownloading;      // 下载状态标记
    QTimer *m_fileDownloadTimer;   // 下载超时定时器

    quint32 m_totalFileSize = 0; // 完整文件包总大小（未使用）
    quint32 m_hdrLen = 0;        // HDR 文件真实长度
    quint32 m_cfgLen = 0;        // CFG 文件真实长度
    quint32 m_datLen = 0;        // DAT 文件真实长度
};

#endif // NORTHCHINA103_H
