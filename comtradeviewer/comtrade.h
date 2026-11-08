#ifndef COMTRADE_H
#define COMTRADE_H

#include <QDateTime>
#include <QString>
#include <QVector>

// 一路模拟量通道（值已按 a*x+b 换算为工程值）
struct CtAnalogChannel {
    QString id;
    QString phase;
    QString unit;
    double a = 1.0;
    double b = 0.0;
    double skew = 0.0;
    double minValue = 0.0;   // CFG 中声明的量程
    double maxValue = 0.0;
    QVector<double> values;  // 每个采样点的工程值
};

// 一路开关量通道（0/1）
struct CtDigitalChannel {
    QString id;
    QString phase;
    QVector<quint8> values;
};

struct CtData {
    QString stationName;
    QString deviceId;
    int revYear = 1999;
    QString fileType;        // ASCII / BINARY / BINARY32 / FLOAT32
    double lineFrequency = 50.0;
    QDateTime startTime;
    QDateTime triggerTime;
    QVector<CtAnalogChannel> analogs;
    QVector<CtDigitalChannel> digitals;
    QVector<double> times;   // 采样时刻（秒，相对第一个采样点）
    double duration = 0.0;
    int sampleCount = 0;

    bool isEmpty() const { return sampleCount <= 0; }
};

class ComtradeReader
{
public:
    // 读取 COMTRADE 的 .cfg，并自动加载同目录同名的 .dat
    static bool read(const QString &cfgPath, CtData &out, QString *error);
};

#endif // COMTRADE_H
