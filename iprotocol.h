#ifndef IPROTOCOL_H
#define IPROTOCOL_H

#include <QObject>
#include <QByteArray>
#include <QVariantMap>
#include "ProtocolDefine.h"
#include <QDateTime>
// 录波文件信息结构体，所有规约通用
struct WaveFileInfo {
    QDateTime faultTime;
    QString fileName;
    quint32 fileSize;
    quint16 deviceAddr;
};
Q_DECLARE_METATYPE(WaveFileInfo)


class IProtocol : public QObject
{
    Q_OBJECT
public:
    explicit IProtocol(QObject *parent = nullptr);
    virtual ~IProtocol() = default;

    // 规约名称
    virtual QString protocolName() const = 0;

    // 统一组帧接口
    virtual QByteArray buildFrame(const QVariantMap& param) = 0;

    // 启动/停止常规轮询（高优先级业务可暂停普通轮询）
    virtual void startPoll() = 0;
    virtual void stopPoll() = 0;
    // 喂入TCP原始数据，内部自动拆帧、解析
    virtual void feedRawData(const QByteArray& data) = 0;

    // 字节流 -> 拆完整帧、解析
    virtual QList<QByteArray> splitRawFrame(const QByteArray& recvBytes) = 0;
    virtual QList<QByteArray> splitRawFrameWithCache(const QByteArray &data, int &usedLen) = 0;
    // 缓存读写虚接口
    virtual QByteArray getRecvBuf() const = 0;
    virtual void setRecvBuf(const QByteArray &buf) = 0;
    virtual void clearRecvBuf() = 0;
    virtual ProtocolParseResult parseFrame(const QByteArray& frame) = 0;
    // 补充：录波文件列表召唤接口（纯虚函数，子类实现）
    virtual void callWaveFileList(const QDateTime& startTime, const QDateTime& endTime) = 0;


signals:

    // 规约层主动请求发送报文，UI层只负责调用TCP发送
    void requestSend(const QByteArray& frame);
    // 解析出一帧完整报文抛出
    void sigOneFrameReady(const QByteArray& frame, MsgDirection dir);
    // 解析到录波列表时发出信号，界面层连接此信号刷新表格
    void waveListReceived(const QList<WaveFileInfo>& waveList);
    // 录波下载进度更新信号
    void waveDownloadProgress(quint32 recvSize, quint32 totalSize);

    void waveListError(const QString &msg);

};

#endif // IPROTOCOL_H
