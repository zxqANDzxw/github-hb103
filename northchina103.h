#ifndef NORTHCHINA103_H
#define NORTHCHINA103_H

#include "iprotocol.h"
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

    // ========== 对外业务接口 ==========
    // 按时间范围召唤录波文件列表
    void callWaveFileList(const QDateTime& startTime, const QDateTime& endTime);

private:
    // ========== 内部工具函数 ==========
    // 固定帧校验和计算
    uchar calcFixedFrameCS(uchar ctrl, uchar addr);
    // 固定帧通用组装
    QByteArray buildFixedFrame(uchar ctrl, uchar addr);
    // 可变帧通用组装
    QByteArray buildVariableFrame(const QByteArray &asduBody, uchar addr);

    // CP56Time2a 编解码（唯一一套，全文件复用）
    QByteArray cp56FromDateTime(const QDateTime& dt);
    QDateTime cp56ToDateTime(const QByteArray& bytes);

    // ASDU 业务解析
    QList<WaveFileInfo> parseWaveListAsdu(const QByteArray& frame);

    // ========== 内部成员变量 ==========
    bool m_fcb;                  // 帧计数位
    QByteArray m_recvBuf;        // 跨包接收缓存
    QTimer *m_pollTimer;         // 二级数据轮询定时器
    QList<WaveFileInfo> m_tempFileList; // 录波列表多包累计缓存

    // 录波文件列表多帧传输
    QList<WaveFileInfo> m_waveListCache;   // 多帧临时缓存
    QTimer *m_waveTransTimer;              // 传输超时定时器
    bool m_isWaveTransmitting;             // 传输状态标记

signals:
    void waveListReceived(const QList<WaveFileInfo> &list);
};

#endif // NORTHCHINA103_H
