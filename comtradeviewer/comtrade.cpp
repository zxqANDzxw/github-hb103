#include "comtrade.h"

#include <QFile>
#include <QFileInfo>
#include <QRegExp>
#include <QTextCodec>
#include <QtEndian>
#include <QPair>
#include <cmath>

namespace {

// CFG 可能是 UTF-8，也可能是 GBK（厂站/通道中文名）
QString decodeText(const QByteArray &raw)
{
    const QString utf8 = QString::fromUtf8(raw);
    if (!utf8.contains(QChar::ReplacementCharacter)) {
        return utf8;
    }
    QTextCodec *codec = QTextCodec::codecForName("GBK");
    return codec ? codec->toUnicode(raw) : utf8;
}

QStringList splitFields(const QString &line)
{
    QStringList parts = line.split(QLatin1Char(','));
    for (int i = 0; i < parts.size(); ++i) {
        parts[i] = parts[i].trimmed();
    }
    return parts;
}

// 从 "96A" / "354D" / "450" 这类文本中取出数字
int numberWithSuffix(const QString &text)
{
    QString digits;
    for (int i = 0; i < text.size(); ++i) {
        if (text.at(i).isDigit()) {
            digits.append(text.at(i));
        }
    }
    return digits.toInt();
}

// 组合 "dd/mm/yyyy" 与 "hh:mm:ss.ssssss"
QDateTime composeDateTime(const QString &dateText, const QString &timeText)
{
    QDate date = QDate::fromString(dateText, QStringLiteral("d/M/yyyy"));
    if (!date.isValid()) {
        date = QDate::fromString(dateText, QStringLiteral("dd/MM/yyyy"));
    }
    const QStringList parts = timeText.split(QLatin1Char(':'));
    if (parts.size() < 3) {
        return QDateTime();
    }
    const double seconds = parts.at(2).toDouble();
    const int whole = int(seconds);
    const int msec = int(std::floor((seconds - whole) * 1000.0 + 0.5));
    return QDateTime(date, QTime(parts.at(0).toInt(), parts.at(1).toInt(), whole, msec));
}

} // namespace

bool ComtradeReader::read(const QString &cfgPath, CtData &out, QString *error)
{
    auto fail = [&](const QString &msg) -> bool {
        if (error) {
            *error = msg;
        }
        return false;
    };

    out = CtData();

    QFile cfgFile(cfgPath);
    if (!cfgFile.open(QIODevice::ReadOnly)) {
        return fail(QStringLiteral("无法打开 CFG 文件：%1").arg(cfgFile.errorString()));
    }
    const QString text = decodeText(cfgFile.readAll());
    cfgFile.close();

    QStringList lines;
    {
        const QStringList raw = text.split(QRegExp(QStringLiteral("\\r?\\n")));
        for (int i = 0; i < raw.size(); ++i) {
            const QString line = raw.at(i).trimmed();
            if (!line.isEmpty()) {
                lines.append(line);
            }
        }
    }
    if (lines.size() < 8) {
        return fail(QStringLiteral("CFG 文件内容不完整"));
    }

    // 第 1 行：厂站名、装置标识、版本年份
    const QStringList meta = splitFields(lines.at(0));
    out.stationName = meta.value(0);
    out.deviceId = meta.value(1);
    out.revYear = meta.value(2).toInt();
    if (out.revYear <= 0) {
        out.revYear = 1999;
    }

    // 第 2 行：总通道数、模拟量数、开关量数（可能是 "450,96A,354D" 形式）
    int idx = 1;
    const QStringList counts = splitFields(lines.at(idx++));
    const int totalCount = numberWithSuffix(counts.value(0));
    int analogCount = numberWithSuffix(counts.value(1));
    int digitalCount = numberWithSuffix(counts.value(2));
    if (analogCount <= 0) {
        analogCount = totalCount;
    }

    // 有些 CFG 把模拟量/开关量数量单独占一行，兼容这种写法
    if (idx < lines.size()) {
        const QStringList t = splitFields(lines.at(idx));
        if (t.size() <= 2) {
            analogCount = numberWithSuffix(t.value(0));
            ++idx;
        }
    }

    for (int i = 0; i < analogCount && idx < lines.size(); ++i) {
        const QStringList p = splitFields(lines.at(idx++));
        CtAnalogChannel ch;
        ch.id = p.value(1);
        ch.phase = p.value(2);
        ch.unit = p.value(4);
        ch.a = p.value(5).toDouble();
        ch.b = p.value(6).toDouble();
        ch.skew = p.value(7).toDouble();
        ch.minValue = p.value(8).toDouble();
        ch.maxValue = p.value(9).toDouble();
        out.analogs.append(ch);
    }

    if (idx < lines.size()) {
        const QStringList t = splitFields(lines.at(idx));
        if (t.size() <= 2) {
            digitalCount = numberWithSuffix(t.value(0));
            ++idx;
        }
    }

    for (int i = 0; i < digitalCount && idx < lines.size(); ++i) {
        const QStringList p = splitFields(lines.at(idx++));
        CtDigitalChannel d;
        d.id = p.value(1);
        d.phase = p.value(2);
        out.digitals.append(d);
    }

    if (idx >= lines.size()) {
        return fail(QStringLiteral("CFG 缺少线路频率"));
    }
    out.lineFrequency = lines.at(idx++).toDouble();
    if (idx >= lines.size()) {
        return fail(QStringLiteral("CFG 缺少采样率信息"));
    }
    const int rateCount = lines.at(idx++).toInt();
    QVector<QPair<double, int> > rates;
    for (int i = 0; i < rateCount && idx < lines.size(); ++i) {
        const QStringList p = splitFields(lines.at(idx++));
        rates.append(qMakePair(p.value(0).toDouble(), p.value(1).toInt()));
    }

    if (idx + 1 < lines.size()) {
        const QStringList st = splitFields(lines.at(idx++));
        out.startTime = composeDateTime(st.value(0), st.value(1));
        const QStringList tr = splitFields(lines.at(idx++));
        out.triggerTime = composeDateTime(tr.value(0), tr.value(1));
    }
    if (idx < lines.size()) {
        out.fileType = lines.at(idx++).trimmed().toUpper();
    }
    if (out.fileType.isEmpty()) {
        out.fileType = QStringLiteral("ASCII");
    }

    // 同名 .dat
    const QFileInfo info(cfgPath);
    QString datPath = info.path() + QLatin1Char('/') + info.completeBaseName() + QStringLiteral(".dat");
    if (!QFile::exists(datPath)) {
        datPath = info.path() + QLatin1Char('/') + info.completeBaseName() + QStringLiteral(".DAT");
    }
    if (!QFile::exists(datPath)) {
        return fail(QStringLiteral("找不到与 CFG 同名的 DAT 文件"));
    }

    const int analogN = out.analogs.size();
    const int digitalN = out.digitals.size();

    // 采样时刻：按 CFG 的采样率分段累加
    auto buildTimes = [&](int samples) {
        out.times.clear();
        out.times.reserve(samples);
        double t = 0.0;
        int seg = 0;
        for (int k = 0; k < samples; ++k) {
            const int sampleNo = k + 1;
            while (seg + 1 < rates.size() && sampleNo > rates.at(seg).second) {
                ++seg;
            }
            double rate = rates.isEmpty() ? 0.0 : rates.at(seg).first;
            if (rate <= 0.0) {
                rate = 2.0 * out.lineFrequency;
            }
            if (k > 0 && rate > 0.0) {
                t += 1.0 / rate;
            }
            out.times.append(t);
        }
        out.sampleCount = samples;
        out.duration = out.times.isEmpty() ? 0.0 : out.times.last();
    };

    if (out.fileType == QStringLiteral("ASCII")) {
        QFile datFile(datPath);
        if (!datFile.open(QIODevice::ReadOnly)) {
            return fail(QStringLiteral("无法打开 DAT 文件：%1").arg(datFile.errorString()));
        }
        const QString datText = decodeText(datFile.readAll());
        datFile.close();
        const QStringList datLines = datText.split(QRegExp(QStringLiteral("\\r?\\n")),
                                                   QString::SkipEmptyParts);

        for (int c = 0; c < analogN; ++c) {
            out.analogs[c].values.reserve(datLines.size());
        }
        for (int c = 0; c < digitalN; ++c) {
            out.digitals[c].values.reserve(datLines.size());
        }

        const QRegExp sep(QStringLiteral("[,;\\s]+"));
        int samples = 0;
        for (int row = 0; row < datLines.size(); ++row) {
            const QString line = datLines.at(row).trimmed();
            if (line.isEmpty()) {
                continue;
            }
            const QStringList parts = line.split(sep, QString::SkipEmptyParts);
            if (parts.size() < 2 + analogN + digitalN) {
                continue;
            }
            for (int c = 0; c < analogN; ++c) {
                CtAnalogChannel &ch = out.analogs[c];
                const double raw = parts.at(2 + c).toDouble();
                ch.values.append(raw * ch.a + ch.b);
            }
            for (int c = 0; c < digitalN; ++c) {
                out.digitals[c].values.append(parts.at(2 + analogN + c).toDouble() >= 0.5 ? 1 : 0);
            }
            ++samples;
        }
        buildTimes(samples);
    } else if (out.fileType == QStringLiteral("BINARY")
               || out.fileType == QStringLiteral("BINARY32")
               || out.fileType == QStringLiteral("FLOAT32")) {
        QFile datFile(datPath);
        if (!datFile.open(QIODevice::ReadOnly)) {
            return fail(QStringLiteral("无法打开 DAT 文件：%1").arg(datFile.errorString()));
        }
        const QByteArray dat = datFile.readAll();
        datFile.close();

        const int words = (digitalN + 15) / 16;
        const int analogBytes = (out.fileType == QStringLiteral("BINARY")) ? 2 : 4;
        const int bytesPerSample = 8 + analogN * analogBytes + words * 2;
        const int samples = bytesPerSample > 0 ? dat.size() / bytesPerSample : 0;
        if (samples <= 0) {
            return fail(QStringLiteral("DAT 中没有有效采样数据"));
        }

        for (int c = 0; c < analogN; ++c) {
            out.analogs[c].values.reserve(samples);
        }
        for (int c = 0; c < digitalN; ++c) {
            out.digitals[c].values.reserve(samples);
        }

        const uchar *base = reinterpret_cast<const uchar *>(dat.constData());
        for (int s = 0; s < samples; ++s) {
            const uchar *sample = base + qint64(s) * bytesPerSample;
            // 前 8 字节为采样序号与时间戳，这里按采样率计算时间
            const uchar *analogPtr = sample + 8;
            for (int c = 0; c < analogN; ++c) {
                CtAnalogChannel &ch = out.analogs[c];
                if (out.fileType == QStringLiteral("BINARY")) {
                    const qint16 raw = qFromLittleEndian<qint16>(analogPtr + c * 2);
                    ch.values.append(double(raw) * ch.a + ch.b);
                } else if (out.fileType == QStringLiteral("BINARY32")) {
                    const qint32 raw = qFromLittleEndian<qint32>(analogPtr + c * 4);
                    ch.values.append(double(raw) * ch.a + ch.b);
                } else {
                    // FLOAT32 中通常已是工程值
                    ch.values.append(double(qFromLittleEndian<float>(analogPtr + c * 4)));
                }
            }

            const uchar *digitalPtr = analogPtr + analogN * analogBytes;
            for (int w = 0; w < words; ++w) {
                const quint16 word = qFromLittleEndian<quint16>(digitalPtr + w * 2);
                for (int b = 0; b < 16; ++b) {
                    const int chIndex = w * 16 + b;
                    if (chIndex >= digitalN) {
                        break;
                    }
                    out.digitals[chIndex].values.append((word >> b) & 1 ? 1 : 0);
                }
            }
        }
        buildTimes(samples);
    } else {
        return fail(QStringLiteral("不支持的 DAT 格式：%1").arg(out.fileType));
    }

    if (out.sampleCount <= 0) {
        return fail(QStringLiteral("没有解析到有效采样点"));
    }
    return true;
}
