#ifndef H103FRAMES_H
#define H103FRAMES_H

#include <QtGlobal>

/*
 * 华北电网 103 规约的字节级布局定义（编码/解码共用）。
 *
 * 对齐与字节序说明：
 *  1. 全部结构体用 #pragma pack(push, 1) 强制 1 字节对齐，并在文件末尾用
 *     static_assert 校验 sizeof，保证不会出现编译器插入的填充字节，
 *     结构体大小与报文实际长度一一对应。
 *  2. 多字节字段（quint32 / quint16）直接使用宿主原生内存布局写入
 *     QByteArray。本工程目标平台为 Windows/x86（小端），与原实现手写
 *     “低字节在前”的 append 顺序完全一致；若将来移植到大端平台，
 *     需要把这些字段改成显式的字节数组。
 *  3. 读取报文时统一用 memcpy 拷贝到结构体（见 northchina103.cpp 的
 *     readStructAt），避免在未对齐地址上直接解引用。
 *  4. 可变帧长度域保存的是“长度值”= 控制域1 + 地址域1 + ASDU 长度，
 *     不包含启动符、长度域自身、校验和与结束符。
 */

#pragma pack(push, 1)

/* ========== 链路层 ========== */

// 固定长度帧：10 | 控制域 | 地址域 | 校验和(异或) | 16，共 5 字节
struct H103FixedFrame
{
    quint8 start;   // 0x10
    quint8 ctrl;    // 控制域
    quint8 addr;    // 地址域
    quint8 cs;      // ctrl ^ addr
    quint8 end;     // 0x16
};

// 可变长度帧头：68 | 长度低 | 长度高 | 68 | 控制域 | 地址域，共 6 字节
struct H103VariableFrameHeader
{
    quint8 start1;  // 0x68
    quint8 lenL;    // 长度值低字节
    quint8 lenH;    // 长度值高字节
    quint8 start2;  // 0x68
    quint8 ctrl;    // 控制域，bit5 同时用作 FCB(发送) / ACD(接收)
    quint8 addr;    // 地址域
};

/* ========== 时间 ========== */

// CP56Time2a：7 字节
struct H103Cp56Time2a
{
    quint8 msL;        // 毫秒低 8 位（毫秒 = 秒*1000 + 毫秒）
    quint8 msH;        // 毫秒高 8 位
    quint8 minute;     // 低 6 位分钟
    quint8 hour;       // 低 5 位小时
    quint8 dayAndWeek; // 低 5 位日，高 3 位星期
    quint8 month;      // 低 4 位月
    quint8 year;       // 低 7 位年（2000 + 值）
};

/* ========== ASDU 公共部分 ========== */

// 通用 ASDU 头：TYP | VSQ | COT | ASDU地址 | FUN | INF，共 6 字节
struct H103AsduHeader
{
    quint8 typ;
    quint8 vsq;
    quint8 cot;
    quint8 asduAddr;
    quint8 fun;
    quint8 inf;
};

// ASDU15 召唤录波文件列表（按时间范围）：6 + 7 + 7 = 20 字节
struct H103AsduWaveListRequest
{
    H103AsduHeader hdr;
    H103Cp56Time2a startTime;
    H103Cp56Time2a endTime;
};

// ASDU13 请求录波文件：6 + 40 字节文件名 + 4 字节起始偏移 = 50 字节
struct H103AsduFileRequest
{
    H103AsduHeader hdr;
    char name[40];   // 超长截断、不足补 0
    quint32 offset;  // 小端
};

// 总召唤 ASDU（原硬编码报文）：6 字节头 + 1 字节 QOI = 7 字节
struct H103AsduGeneralInterrogation
{
    H103AsduHeader hdr;
    quint8 qoi;
};

// ASDU14 文件分段数据头（注意：本规约此处只有 4 字节 ASDU 前缀）= 8 字节
struct H103AsduFileDataHeader
{
    quint8 typ;
    quint8 vsq;
    quint8 cot;
    quint8 asduAddr;
    quint32 offset;  // 本段数据在完整文件包中的起始位置，小端
};

// ASDU16 录波文件列表应答头（本规约自定义 20 字节）
// 原实现从 ASDU 偏移 4 起按小端读取 2 字节文件数，条目从偏移 20 开始
struct H103AsduWaveListHead
{
    quint8 typ;
    quint8 vsq;
    quint8 cot;
    quint8 asduAddr;
    quint16 fileCount;   // 小端
    quint8 reserved[14]; // 其余字节本程序未使用，保持原偏移
};

// 录波文件列表条目：1 字节装置地址 + 40 字节文件名 + 7 字节故障时间 = 48 字节
struct H103WaveFileItem
{
    quint8 deviceAddr;
    char name[40];
    H103Cp56Time2a faultTime;
};

/* ========== COMTRADE 文件包 ========== */

// 文件包公共头：40 字节文件名 + 7 字节故障时间 = 47 字节
struct H103ComtradePackageHeader
{
    char name[40];
    H103Cp56Time2a faultTime;
};

// 文件块头：1 字节类型（1=HDR, 2=CFG, 3=DAT）+ 4 字节长度（小端）= 5 字节
struct H103FileBlockHeader
{
    quint8 type;
    quint32 len;
};

#pragma pack(pop)

/* ========== 布局校验（编译期） ========== */

static_assert(sizeof(H103FixedFrame) == 5, "H103FixedFrame 必须为 5 字节");
static_assert(sizeof(H103VariableFrameHeader) == 6, "H103VariableFrameHeader 必须为 6 字节");
static_assert(sizeof(H103Cp56Time2a) == 7, "H103Cp56Time2a 必须为 7 字节");
static_assert(sizeof(H103AsduHeader) == 6, "H103AsduHeader 必须为 6 字节");
static_assert(sizeof(H103AsduWaveListRequest) == 20, "H103AsduWaveListRequest 必须为 20 字节");
static_assert(sizeof(H103AsduFileRequest) == 50, "H103AsduFileRequest 必须为 50 字节");
static_assert(sizeof(H103AsduGeneralInterrogation) == 7, "H103AsduGeneralInterrogation 必须为 7 字节");
static_assert(sizeof(H103AsduFileDataHeader) == 8, "H103AsduFileDataHeader 必须为 8 字节");
static_assert(sizeof(H103AsduWaveListHead) == 20, "H103AsduWaveListHead 必须为 20 字节");
static_assert(sizeof(H103WaveFileItem) == 48, "H103WaveFileItem 必须为 48 字节");
static_assert(sizeof(H103ComtradePackageHeader) == 47, "H103ComtradePackageHeader 必须为 47 字节");
static_assert(sizeof(H103FileBlockHeader) == 5, "H103FileBlockHeader 必须为 5 字节");

#endif // H103FRAMES_H
