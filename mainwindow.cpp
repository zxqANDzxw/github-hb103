#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "adddevicedialog.h"
#include "wavemanagedialog.h"
#include <QMessageBox>
#include <QMenu>
#include <QAction>
#include <QTextCursor>
#include <QTextBlock>
#include <QTextDocument>
#include <QTextFormat>
#include <QStatusBar>
#include <QFontMetrics>
#include <QHeaderView>
#include <QStringList>
#include "util.h"

// 全局指针实体定义
QPlainTextEdit *g_logEdit = nullptr;


MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow)
{
    ui->setupUi(this);
    // 赋值全局指针，指向界面文本框
    g_logEdit = ui->plainTextEdit;
    // 选择报文时，按“整帧（含子站分帧/ TCP 分片）”吸附
    ui->plainTextEdit->setGroupResolver([this](int blockNumber){
        return messageGroupForBlock(blockNumber);
    });
    // 点中解析表格的某一行时，在报文收发窗口把对应字节标绿
    connect(ui->parseTable, &QTableWidget::cellClicked, this, [this](int row, int){
        highlightLogByteForParseRow(row);
    });
    // 报文收发窗口选中内容变化时，状态栏提示选中报文的总字节数
    connect(ui->plainTextEdit, &QPlainTextEdit::selectionChanged,
            this, &MainWindow::updateSelectionByteHint);
    // 给水平分割器设置比例：设备列表 : 报文收发 = 1 : 3
    ui->splitter->setStretchFactor(0, 1);  // 设备列表（第0个控件）
    ui->splitter->setStretchFactor(1, 3);  // 报文收发窗口（第1个控件）

    // 给垂直分割器设置比例：上半部分 : 解析窗口 = 3 : 1
    ui->splitter_2->setStretchFactor(0, 3); // 上半部分整体
    ui->splitter_2->setStretchFactor(1, 1); // 报文解析窗口

    // 1. 初始化 TCP
    m_tcp = new TcpClient(this);

    // 2. 初始化 华北103
    m_protocol = new NorthChina103(this);

    // 底部页签：报文解析 + 录波文件管理；录波面板常驻，只创建一次
    m_waveDialog = new WaveManageDialog(this, m_tcp, m_protocol, ui->plainTextEdit);
    ui->tabWidget->addTab(m_waveDialog, QStringLiteral("录波文件管理"));

    // 3. 建立右键菜单命令表
    initDeviceCommands();

    // 连接成功后启动轮询，不用手动管理定时器
    connect(m_tcp, &TcpClient::sigConnected, this, [=]{
        m_protocol->startPoll();
    });

    // tcp收到数据后数据帧处理，只喂给规约层
    connect(m_tcp, &TcpClient::sigRecvRawData, this, [=](const QByteArray& data){
        printRecvLog(data, ui->plainTextEdit);
        m_protocol->feedRawData(data);
    });

    // 规约层请求发送：统一调用TCP
    connect(m_protocol, &IProtocol::requestSend, this, [=](const QByteArray& frame){
        m_tcp->sendRawData(frame);
        printSendLog(frame, ui->plainTextEdit);
    });
}

MainWindow::~MainWindow()
{
    delete ui;
}

/* ============================================================
 * 命令表：菜单文字 -> 处理函数
 *
 * 注意（既有 bug，保持原样）：
 *   原实现菜单项文字为 "总召唤C_IGI_NA_3"，而分支判断写的是 "总召唤"，
 *   两者不相等，导致该菜单项点击后没有任何动作。
 *   为了保持行为一致，命令表里仍然登记 "总召唤" 这个永不命中的 key，
 *   请勿"顺手修复"为 "总召唤C_IGI_NA_3"。
 *   同理："断开设备"、"信号复归"、"定值操作"在菜单里存在但原实现无分支，
 *   因此命令表不登记，点击后仍然无动作。
 * ============================================================ */
void MainWindow::initDeviceCommands()
{
    m_deviceCommands.insert("连接设备", [this](){ cmdConnectDevice(); });

    m_deviceCommands.insert("复位通信单元C_RCU_NA_3", [this](){ cmdResetCommUnit(); });
    m_deviceCommands.insert("复位帧计数位C_RFB_NA_3", [this](){ cmdResetFcb(); });

    // 既有 bug：key 与菜单文字不一致，永远不会命中（见上方说明）
    m_deviceCommands.insert("总召唤", [this](){ cmdGeneralInterrogation(); });

    m_deviceCommands.insert("召唤一级数据C_PL1_NA_3", [this](){ cmdSummonLevel1(); });
    m_deviceCommands.insert("召唤二级数据C_PL2_NA_3", [this](){ cmdSummonLevel2(); });

    // 原实现对应分支为空，这里保留空处理
    m_deviceCommands.insert("装置对时C_SYN_TA_3", [this](){ cmdTimeSyncNoop(); });

    m_deviceCommands.insert("录波操作", [this](){ cmdOpenWaveDialog(); });
    m_deviceCommands.insert("编辑设备", [this](){ cmdEditDevice(); });
    m_deviceCommands.insert("删除设备", [this](){ cmdDeleteDevice(); });
}

QTreeWidgetItem *MainWindow::currentDeviceItem() const
{
    return ui->treeWidget->currentItem();
}

void MainWindow::sendProtocolFrame(const QString &type, int addr, const QString &errTip)
{
    if (!m_tcp->isConnected()) {
        ui->plainTextEdit->appendPlainText("请先连接设备！");
        return;
    }

    QVariantMap param;
    param["type"] = type;
    if (addr >= 0) {
        param["addr"] = addr;
    }

    QByteArray frame = m_protocol->buildFrame(param);
    if (frame.isEmpty()) {
        ui->plainTextEdit->appendPlainText(errTip);
        return;
    }

    m_tcp->sendRawData(frame);
    printSendLog(frame, ui->plainTextEdit);
}

/* ============================================================
 * 设备项右键菜单：原菜单结构保持不变，动作通过命令表分发
 * ============================================================ */
void MainWindow::showDeviceMenu(const QPoint &pos)
{
    QMenu menu;
    menu.addAction("连接设备");
    menu.addAction("断开设备");
    menu.addSeparator();
    menu.addAction("复位通信单元C_RCU_NA_3");
    menu.addAction("复位帧计数位C_RFB_NA_3");
    menu.addAction("总召唤C_IGI_NA_3");
    menu.addAction("召唤一级数据C_PL1_NA_3");
    menu.addAction("召唤二级数据C_PL2_NA_3");
    menu.addSeparator();
    menu.addAction("装置对时C_SYN_TA_3");
    menu.addAction("信号复归");
    menu.addSeparator();
    menu.addAction("录波操作");
    menu.addAction("定值操作");
    menu.addSeparator();
    menu.addAction("编辑设备");
    menu.addAction("删除设备");

    QAction *act = menu.exec(ui->treeWidget->mapToGlobal(pos));
    if (!act) {
        return;
    }

    auto it = m_deviceCommands.constFind(act->text());
    if (it != m_deviceCommands.constEnd()) {
        it.value()();
    }
}

/* ============================================================
 * 空白处右键菜单：添加设备
 * ============================================================ */
void MainWindow::showBlankMenu(const QPoint &pos)
{
    QMenu menu;
    QAction *addDevice = menu.addAction("添加设备");
    QAction *act = menu.exec(ui->treeWidget->mapToGlobal(pos));

    if (act != addDevice) {
        return;
    }

    AddDeviceDialog dialog(this);
    dialog.setWindowTitle("添加设备");
    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    // 读取用户输入的配置
    QString name = dialog.deviceName();
    QString ip = dialog.ipAddress();
    int port = dialog.port();

    if (port < 1 || port > 65535) {
        QMessageBox::warning(this, "错误", "端口号必须在1-65535之间！");
        return;
    }

    // 设备名空的话，给个默认值
    if (name.isEmpty()) {
        name = QString("新设备(%1:%2)").arg(ip).arg(port);
    }

    // 添加到列表里
    QTreeWidgetItem *newItem = new QTreeWidgetItem(ui->treeWidget);
    newItem->setText(0, name);
    // 把IP和端口存到Item里，后面通信要用
    newItem->setData(0, Qt::UserRole + 1, ip);
    newItem->setData(0, Qt::UserRole + 2, port);
}

void MainWindow::on_treeWidget_customContextMenuRequested(const QPoint &pos)
{
    QTreeWidgetItem *item = ui->treeWidget->itemAt(pos);
    if (item) {
        showDeviceMenu(pos);
    } else {
        showBlankMenu(pos);
    }
}

/* ============================================================
 * 各命令处理：逐条对应原 if/else 分支，行为与提示文字保持一致
 * ============================================================ */
void MainWindow::cmdConnectDevice()
{
    QTreeWidgetItem *item = currentDeviceItem();
    if (!item) {
        return;
    }

    // 读取设备配置信息（添加/编辑时通过 setData 存入）
    QString ip = item->data(0, Qt::UserRole + 1).toString();
    int port = item->data(0, Qt::UserRole + 2).toInt();

    // 断开之前的连接（如果有）
    if (m_tcp->isConnected()) {
        m_tcp->disconnectTcp();
    }

    if (!m_tcp->connectTcp(ip, port)) {
        ui->plainTextEdit->appendPlainText(QString("=== 连接设备失败：%1:%2 ===").arg(ip).arg(port));
    }
}

void MainWindow::cmdResetCommUnit()
{
    sendProtocolFrame("复位通信单元", 1, "错误：构建复位通信单元帧失败！");
}

void MainWindow::cmdResetFcb()
{
    sendProtocolFrame("复位帧计数位", 1, "错误：构建复位帧计数位帧失败！");
}

void MainWindow::cmdGeneralInterrogation()
{
    sendProtocolFrame("总召唤", -1, "错误：构建总召唤帧失败！");
}

void MainWindow::cmdSummonLevel1()
{
    // 原实现失败提示文字就是“构建总召唤帧失败”，保持原样
    sendProtocolFrame("召唤一级数据", -1, "错误：构建总召唤帧失败！");
}

void MainWindow::cmdSummonLevel2()
{
    // 原实现失败提示文字就是“构建总召唤帧失败”，保持原样
    sendProtocolFrame("召唤二级数据", -1, "错误：构建总召唤帧失败！");
}

void MainWindow::cmdTimeSyncNoop()
{
    // 原实现对应分支为空，不做任何处理
}

void MainWindow::cmdOpenWaveDialog()
{
    // 录波文件管理已并入底部页签，这里直接切到该页
    ui->tabWidget->setCurrentWidget(m_waveDialog);
}

void MainWindow::cmdEditDevice()
{
    QTreeWidgetItem *item = currentDeviceItem();
    if (!item) {
        return;
    }

    // 读取设备现有的配置信息
    QString oldName = item->text(0);
    QString oldIp = item->data(0, Qt::UserRole + 1).toString();
    int oldPort = item->data(0, Qt::UserRole + 2).toInt();

    // 复用 AddDeviceDialog，把旧配置填进去
    AddDeviceDialog dialog(this);
    dialog.setWindowTitle("编辑设备");
    dialog.setDeviceName(oldName);
    dialog.setIpAddress(oldIp);
    dialog.setPort(oldPort);

    if (dialog.exec() != QDialog::Accepted) {
        return;
    }

    QString newName = dialog.deviceName();
    QString newIp = dialog.ipAddress();
    int newPort = dialog.port();
    if (newPort < 1 || newPort > 65535) {
        QMessageBox::warning(this, "错误", "端口号必须在1-65535之间！");
        return;
    }

    item->setText(0, newName.isEmpty() ? oldName : newName);
    item->setData(0, Qt::UserRole + 1, newIp);
    item->setData(0, Qt::UserRole + 2, newPort);
}

void MainWindow::cmdDeleteDevice()
{
    QTreeWidgetItem *item = currentDeviceItem();
    if (!item) {
        return;
    }

    // 弹出确认框，防止误删
    QMessageBox::StandardButton reply =
        QMessageBox::question(this, "确认删除",
                              QString("确定要删除设备「%1」吗？").arg(item->text(0)),
                              QMessageBox::Yes | QMessageBox::No);
    if (reply == QMessageBox::Yes) {
        delete item; // 直接删除列表项
    }
}

namespace {

// 从头扫描一段字节流，判断是否为完整帧序列，并取出末帧的 ACD 与装置地址
void scanFrameStream(const QByteArray &data, bool *startsAtFrame, bool *complete,
                     bool *lastFrameAcd, int *deviceAddr)
{
    *startsAtFrame = false;
    *complete = false;
    *lastFrameAcd = false;
    *deviceAddr = -1;
    if (data.isEmpty()) {
        return;
    }

    const quint8 first = quint8(data.at(0));
    *startsAtFrame = (first == 0x10 || first == 0x68);

    bool anyFrame = false;
    int pos = 0;
    while (pos < data.size()) {
        const quint8 b = quint8(data.at(pos));

        // 固定帧：10 控制域 地址域 校验 16
        if (b == 0x10) {
            if (pos + 5 > data.size() || quint8(data.at(pos + 4)) != 0x16) {
                return;
            }
            *lastFrameAcd = (quint8(data.at(pos + 1)) & 0x20) != 0;
            *deviceAddr = int(quint8(data.at(pos + 2)));
            anyFrame = true;
            pos += 5;
            continue;
        }

        // 可变帧：68 长度低 长度高 68 控制域 地址域 ... 校验 16
        if (b == 0x68) {
            if (pos + 4 > data.size() || quint8(data.at(pos + 3)) != 0x68) {
                return;
            }
            const int bodyLen = int(quint16((quint16(quint8(data.at(pos + 2))) << 8)
                                            | quint16(quint8(data.at(pos + 1)))));
            const int frameLen = 4 + bodyLen + 2;
            if (bodyLen < 2 || pos + frameLen > data.size()
                || quint8(data.at(pos + frameLen - 1)) != 0x16) {
                return;
            }
            *lastFrameAcd = (quint8(data.at(pos + 4)) & 0x20) != 0;
            *deviceAddr = int(quint8(data.at(pos + 5)));
            anyFrame = true;
            pos += frameLen;
            continue;
        }

        // 非帧起始字节：视为分片或不合法报文
        return;
    }
    *complete = anyFrame;
}

bool isHexDigit(QChar ch)
{
    const QChar u = ch.toUpper();
    return (u >= QLatin1Char('0') && u <= QLatin1Char('9'))
        || (u >= QLatin1Char('A') && u <= QLatin1Char('F'));
}

// 日志行中第 byteIndex 个字节（形如 "68"）的字符起始位置
int hexTokenCharPos(const QString &text, int colon, int byteIndex)
{
    int count = 0;
    int i = colon + 1;
    while (i + 1 < text.size()) {
        if (text.at(i).isSpace()) {
            ++i;
            continue;
        }
        if (!isHexDigit(text.at(i)) || !isHexDigit(text.at(i + 1))) {
            return -1;
        }
        if (count == byteIndex) {
            return i;
        }
        ++count;
        i += 2;
    }
    return -1;
}

} // namespace

// 向“报文解析窗口”追加一行说明（横跨两列）；byteStart/byteCount 记录该行对应的字节区间
void MainWindow::appendParseLine(const QString &text, int byteStart, int byteCount)
{
    const int row = ui->parseTable->rowCount();
    ui->parseTable->insertRow(row);

    QTableWidgetItem *item = new QTableWidgetItem(text);
    item->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable); // 只读、可选中
    item->setData(Qt::UserRole, byteStart);
    item->setData(Qt::UserRole + 1, byteCount);
    if (text.startsWith(QStringLiteral("=== 华北103 报文解析"))) {
        QFont f = ui->parseTable->font();
        f.setBold(true);
        item->setFont(f);
        item->setForeground(QColor(20, 70, 140));
    }
    ui->parseTable->setItem(row, 0, item);
    // 标题、字段汇总、说明等整行内容横跨两列，避免被“报文原文”列挤断
    ui->parseTable->setSpan(row, 0, 1, ui->parseTable->columnCount());
}

// 向“报文解析窗口”追加一条字节明细：左列原文（偏移 + 字节），右列解析说明
void MainWindow::appendParseDetail(const QString &original, const QString &desc,
                                   int byteStart, int byteCount)
{
    const int row = ui->parseTable->rowCount();
    ui->parseTable->insertRow(row);

    QTableWidgetItem *srcItem = new QTableWidgetItem(original);
    srcItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable); // 只读、可选中
    srcItem->setData(Qt::UserRole, byteStart);
    srcItem->setData(Qt::UserRole + 1, byteCount);
    ui->parseTable->setItem(row, 0, srcItem);

    QTableWidgetItem *descItem = new QTableWidgetItem(desc);
    descItem->setFlags(Qt::ItemIsEnabled | Qt::ItemIsSelectable);
    ui->parseTable->setItem(row, 1, descItem);
}

void MainWindow::on_plainTextEdit_customContextMenuRequested(const QPoint &pos)
{
    QMenu menu;
    QAction *parseAction = menu.addAction("解析");
    QAction *chosen = menu.exec(ui->plainTextEdit->mapToGlobal(pos));
    if (chosen != parseAction) {
        return;
    }
    parseLogMessagesAt(pos);
}

void MainWindow::parseLogMessagesAt(const QPoint &pos)
{
    QTextCursor selected = ui->plainTextEdit->textCursor();
    QTextCursor hit = ui->plainTextEdit->cursorForPosition(pos);
    QTextDocument *logDoc = ui->plainTextEdit->document();

    // 右键落在选区内：解析整段选区；否则解析右键所在行所属的整条逻辑报文
    QStringList lines;
    QList<int> lineBlocks;
    const bool useSelection = selected.hasSelection()
                              && hit.position() >= selected.selectionStart()
                              && hit.position() <= selected.selectionEnd();
    if (useSelection) {
        // 选区已吸附到整行，按块遍历并保留行号（用于字节定位）
        QTextBlock lastBlock = logDoc->findBlock(selected.selectionEnd());
        for (QTextBlock b = logDoc->findBlock(selected.selectionStart());
             b.isValid(); b = b.next()) {
            lines << b.text();
            lineBlocks << b.blockNumber();
            if (b == lastBlock) {
                break;
            }
        }
    } else {
        const int blockNo = hit.block().blockNumber();
        const QPair<int, int> group = messageGroupForBlock(blockNo);
        const int lo = (group.first >= 0) ? group.first : blockNo;
        const int hi = (group.second >= 0) ? group.second : blockNo;
        for (int i = lo; i <= hi; ++i) {
            QTextBlock b = logDoc->findBlockByNumber(i);
            if (b.isValid()) {
                lines << b.text();
                lineBlocks << i;
            }
        }
    }

    // 解析窗口每次只显示本次解析的结果
    ui->parseTable->setRowCount(0);
    m_parsedSources.clear();
    m_parsedByteBase = 0;
    m_parsedGrey.clear();
    QList<QTextEdit::ExtraSelection> noMark;
    ui->plainTextEdit->setExtraSelections(noMark);

    // 相邻且方向相同的行拼成一条字节流：同一条逻辑报文不会跨发送/接收
    QString caption;
    QString direction;
    QByteArray raw;
    int lineCount = 0;
    QList<LogByteSource> sources;

    auto flush = [&](){
        if (lineCount > 0) {
            appendParsedStream(caption, lineCount, raw, sources);
        }
        caption.clear();
        direction.clear();
        raw.clear();
        sources.clear();
        lineCount = 0;
    };

    for (int i = 0; i < lines.size(); ++i) {
        const QString line = lines.at(i).trimmed();
        if (line.isEmpty()) {
            continue;
        }

        const LogLineInfo info = analyzeLogLine(line);
        if (!info.valid) {
            flush();
            appendParseLine(QString("【%1】未包含可解析的十六进制报文").arg(line));
            appendParseLine(QString());
            continue;
        }
        if (lineCount > 0 && info.direction != direction) {
            flush();
        }
        if (lineCount == 0) {
            caption = info.caption;
            direction = info.direction;
        }

        LogByteSource src;
        src.blockNumber = lineBlocks.at(i);
        src.startByte = raw.size();
        src.byteCount = info.bytes.size();
        sources << src;

        raw += info.bytes;
        ++lineCount;

        // 合并上限，避免异常日志把整屏内容都拼进来
        if (lineCount >= 64) {
            flush();
        }
    }
    flush();

    // 列宽按内容自适应（之后仍可拖动表头分隔线手动调整）
    autoFitParseColumns();

    // 被解析的报文在报文收发窗口整段标灰
    applyParsedHighlight();
}

MainWindow::LogLineInfo MainWindow::analyzeLogLine(const QString &line) const
{
    LogLineInfo info;

    // 日志行格式：hh:mm:ss 发送 / 接收：68 16 00 ...
    QString hexPart = line;
    const int colon = line.indexOf(QChar(0xFF1A)); // 全角冒号“：”
    if (colon >= 0) {
        info.caption = line.left(colon).trimmed();
        if (info.caption.contains(QStringLiteral("发送"))) {
            info.direction = QStringLiteral("发送");
        } else if (info.caption.contains(QStringLiteral("接收"))) {
            info.direction = QStringLiteral("接收");
        }
        hexPart = line.mid(colon + 1);
    }

    // 只接受“十六进制字符 + 空白”，避免把提示文字误当成报文
    QString hex;
    for (int i = 0; i < hexPart.size(); ++i) {
        const QChar ch = hexPart.at(i).toUpper();
        if (ch.isSpace()) {
            continue;
        }
        if ((ch >= QLatin1Char('0') && ch <= QLatin1Char('9'))
            || (ch >= QLatin1Char('A') && ch <= QLatin1Char('F'))) {
            hex.append(ch);
            continue;
        }
        return info;
    }

    if (hex.isEmpty() || hex.size() % 2 != 0) {
        return info;
    }

    info.valid = true;
    info.bytes = QByteArray::fromHex(hex.toLatin1());
    scanFrameStream(info.bytes, &info.startsAtFrame, &info.complete,
                    &info.lastFrameAcd, &info.deviceAddr);
    return info;
}

// 状态栏提示：当前选中的报文共有多少字节（按选中行统计十六进制字节）
void MainWindow::updateSelectionByteHint()
{
    QTextCursor cursor = ui->plainTextEdit->textCursor();
    if (!cursor.hasSelection()) {
        statusBar()->clearMessage();
        return;
    }

    QTextDocument *doc = ui->plainTextEdit->document();
    QTextBlock first = doc->findBlock(cursor.selectionStart());
    QTextBlock last = doc->findBlock(cursor.selectionEnd());
    if (!first.isValid() || !last.isValid()) {
        statusBar()->clearMessage();
        return;
    }

    int totalBytes = 0;
    int lineCount = 0;
    for (QTextBlock b = first; b.isValid(); b = b.next()) {
        const LogLineInfo info = analyzeLogLine(b.text().trimmed());
        if (info.valid) {
            totalBytes += info.bytes.size();
        }
        ++lineCount;
        if (b == last) {
            break;
        }
    }

    if (totalBytes <= 0) {
        statusBar()->clearMessage();
        return;
    }
    statusBar()->showMessage(QString("已选中 %1 字节（%2 行）").arg(totalBytes).arg(lineCount));
}

// 按内容自动调整“报文解析”两列的默认宽度（之后仍可手动拖动）
void MainWindow::autoFitParseColumns()
{
    if (ui->parseTable->rowCount() <= 0) {
        return;
    }

    const QFontMetrics fm(ui->parseTable->font());
    int w0 = 0; // 报文原文列
    int w1 = 0; // 报文解析列
    if (QTableWidgetItem *h = ui->parseTable->horizontalHeaderItem(0)) {
        w0 = fm.width(h->text());
    }
    if (QTableWidgetItem *h = ui->parseTable->horizontalHeaderItem(1)) {
        w1 = fm.width(h->text());
    }

    for (int r = 0; r < ui->parseTable->rowCount(); ++r) {
        // 横跨两列的标题/说明行不参与单列宽度计算，否则会把单列撑得过宽
        if (ui->parseTable->columnSpan(r, 0) > 1) {
            continue;
        }
        if (QTableWidgetItem *item = ui->parseTable->item(r, 0)) {
            w0 = qMax(w0, fm.width(item->text()));
        }
        if (QTableWidgetItem *item = ui->parseTable->item(r, 1)) {
            w1 = qMax(w1, fm.width(item->text()));
        }
    }

    w0 += 16; // 左右留白
    w1 += 16;
    ui->parseTable->setColumnWidth(0, w0);
    ui->parseTable->setColumnWidth(1, w1);

    // 内容不足一屏时把余量给“报文解析”列，避免右侧留白
    const int avail = ui->parseTable->viewport()->width();
    if (w0 + w1 < avail) {
        ui->parseTable->setColumnWidth(1, w1 + (avail - w0 - w1));
    }
}

QPair<int, int> MainWindow::messageGroupForBlock(int blockNumber) const
{
    QTextDocument *doc = ui->plainTextEdit->document();
    QTextBlock block = doc->findBlockByNumber(blockNumber);
    if (!block.isValid() || block.text().trimmed().isEmpty()) {
        return QPair<int, int>(-1, -1);
    }

    const int maxSpan = 64; // 单条逻辑报文的合并上限（行）

    auto lineAt = [doc](int n) -> QString {
        QTextBlock b = doc->findBlockByNumber(n);
        return b.isValid() ? b.text().trimmed() : QString();
    };

    int lo = blockNumber;
    int hi = blockNumber;

    // ---- 向前扩展：本行是续片，或上一行还有后续帧（ACD） ----
    while (lo > 0 && blockNumber - lo < maxSpan) {
        const LogLineInfo cur = analyzeLogLine(lineAt(lo));
        if (!cur.valid) {
            break;
        }
        const LogLineInfo prev = analyzeLogLine(lineAt(lo - 1));
        if (!prev.valid || prev.direction != cur.direction) {
            break;
        }

        if (!cur.startsAtFrame) {
            // 不是帧起始的续片：只有上一行尚未收完整才并入
            if (prev.complete) {
                break;
            }
            --lo;
            continue;
        }
        if (!prev.complete) {
            --lo; // 上一行没收完，本行是它后面的字节
            continue;
        }
        if (cur.direction == QStringLiteral("接收") && prev.lastFrameAcd
            && (prev.deviceAddr < 0 || cur.deviceAddr < 0
                || prev.deviceAddr == cur.deviceAddr)) {
            --lo; // 子站分帧上送：上一帧 ACD=1，还有后续帧
            continue;
        }
        break;
    }

    // ---- 向后扩展：累计字节未收完，或末帧 ACD=1 ----
    while (hi + 1 < doc->blockCount() && hi - blockNumber < maxSpan) {
        const LogLineInfo cur = analyzeLogLine(lineAt(hi));
        if (!cur.valid) {
            break;
        }
        const LogLineInfo next = analyzeLogLine(lineAt(hi + 1));
        if (!next.valid || next.direction != cur.direction) {
            break;
        }

        // 从 lo 到 hi 的累计字节流（lo 已经在向前扩展时确定）
        QByteArray acc;
        for (int k = lo; k <= hi; ++k) {
            acc += analyzeLogLine(lineAt(k)).bytes;
        }
        bool startsAtFrame = false;
        bool complete = false;
        bool lastFrameAcd = false;
        int deviceAddr = -1;
        scanFrameStream(acc, &startsAtFrame, &complete, &lastFrameAcd, &deviceAddr);

        if (!complete) {
            // 还没收完整帧；下一行若是新帧起点，说明这一帧已经断裂
            if (next.startsAtFrame) {
                break;
            }
            ++hi;
            continue;
        }
        if (cur.direction == QStringLiteral("接收") && lastFrameAcd) {
            if (next.startsAtFrame && deviceAddr >= 0 && next.deviceAddr >= 0
                && next.deviceAddr != deviceAddr) {
                break; // 换了装置，不是同一条逻辑报文
            }
            ++hi;
            continue;
        }
        break;
    }

    return QPair<int, int>(lo, hi);
}

void MainWindow::appendParsedStream(const QString &caption, int lineCount,
                                    const QByteArray &raw, const QList<LogByteSource> &sources)
{
    if (raw.isEmpty()) {
        return;
    }

    // 本段字节流在“解析 → 日志”映射中的绝对起点；多段解析依次递增，互不覆盖
    const int segmentBase = m_parsedByteBase;
    m_parsedByteBase += raw.size();

    // 记录本段涉及的日志行，起始偏移统一换算成绝对偏移
    for (int i = 0; i < sources.size(); ++i) {
        LogByteSource src = sources.at(i);
        src.startByte += segmentBase;
        m_parsedSources << src;
    }

    // 一条逻辑报文的字节流可能由多行拼接而成，先按规约拆成完整帧
    const QList<QByteArray> frames = m_protocol->splitRawFrame(raw);
    if (frames.isEmpty()) {
        appendParseLine(QString("【%1】未识别出完整的华北103帧").arg(caption));
        appendParseLine(QStringLiteral("说明：该行可能是不完整的 TCP 片段，或不是本规约报文。"));
        appendParseLine(QString());
        return;
    }

    int searchFrom = 0;
    for (int i = 0; i < frames.size(); ++i) {
        const QByteArray &frame = frames.at(i);
        const ProtocolParseResult r = m_protocol->parseFrame(frame);
        int at = raw.indexOf(frame, searchFrom);
        if (at < 0) {
            at = searchFrom;
        }
        searchFrom = at + frame.size();

        QString header = QStringLiteral("=== 华北103 报文解析");
        if (!caption.isEmpty()) {
            header += QString("（%1）").arg(caption);
        }
        if (lineCount > 1) {
            header += QString(" 合并 %1 行").arg(lineCount);
        }
        if (frames.size() > 1) {
            header += QString(" 第 %1/%2 帧").arg(i + 1).arg(frames.size());
        }
        header += QStringLiteral(" ===");

        // 原始报文不再重复显示：它已在报文收发窗口整段灰色高亮
        appendParseLine(header);

        const QStringList detailLines = r.detailDesc.split(QChar('\n'), QString::SkipEmptyParts);
        // 先取出每条明细行的帧内字节偏移（[NN] 中的 NN 是十进制）
        QList<int> offsets;
        for (int k = 0; k < detailLines.size(); ++k) {
            const QString text = detailLines.at(k).trimmed();
            int offset = -1;
            if (text.startsWith(QLatin1Char('['))) {
                const int close = text.indexOf(QLatin1Char(']'));
                if (close > 1) {
                    bool ok = false;
                    const int v = text.mid(1, close - 1).trimmed().toInt(&ok, 10);
                    if (ok) {
                        offset = v;
                    }
                }
            }
            offsets << offset;
        }

        for (int k = 0; k < detailLines.size(); ++k) {
            const int offset = offsets.at(k);
            const QString line = detailLines.at(k);
            int byteStart = -1;
            int byteCount = 0;
            if (offset >= 0 && offset < frame.size()) {
                // 字段长度 = 下一条更大的偏移 - 本条偏移；
                // 即使显示时用“...”截断，也能覆盖被解析字段的完整字节
                int nextOffset = -1;
                for (int n = k + 1; n < offsets.size(); ++n) {
                    if (offsets.at(n) > offset) {
                        nextOffset = offsets.at(n);
                        break;
                    }
                }
                byteCount = (nextOffset > offset && nextOffset <= frame.size())
                            ? nextOffset - offset : 1;
                byteStart = segmentBase + at + offset;
            }

            if (offset < 0) {
                // 说明行（如“第 N 个文件条目（偏移 …）”）：整行显示
                appendParseLine(line, byteStart, byteCount);
                continue;
            }

            // 明细行格式 "[NN] <字节串>  <说明>"：左边放偏移 + 字节，右边放说明
            const QString text = line.trimmed();
            const int close = text.indexOf(QLatin1Char(']'));
            QString originalPart = text;
            QString descPart;
            if (close > 0) {
                const QString rest = text.mid(close + 1);
                // 字节串内部只用单空格分隔，因此“两个连续空格”即为字节串与说明的分界
                int sep = -1;
                for (int p = 0; p + 1 < rest.size(); ++p) {
                    if (rest.at(p) == QLatin1Char(' ') && rest.at(p + 1) == QLatin1Char(' ')) {
                        sep = p;
                        break;
                    }
                }
                if (sep >= 0) {
                    originalPart = (text.left(close + 1) + rest.left(sep)).trimmed();
                    descPart = rest.mid(sep).trimmed();
                }
            }
            appendParseDetail(originalPart, descPart, byteStart, byteCount);
        }

        if (!r.ok) {
            appendParseLine(QStringLiteral("    （结论：报文结构或校验异常，请核对上方标注）"));
        }
        appendParseLine(QString());
    }
}

// 把本次解析用到的日志行全部标为灰色（“被解析报文”的定位标记）
void MainWindow::applyParsedHighlight()
{
    m_parsedGrey.clear();

    QList<int> blocks;
    for (int i = 0; i < m_parsedSources.size(); ++i) {
        const int blockNumber = m_parsedSources.at(i).blockNumber;
        if (blockNumber >= 0 && !blocks.contains(blockNumber)) {
            blocks << blockNumber;
        }
    }

    QTextDocument *doc = ui->plainTextEdit->document();
    for (int i = 0; i < blocks.size(); ++i) {
        QTextBlock b = doc->findBlockByNumber(blocks.at(i));
        if (!b.isValid() || b.length() <= 1) {
            continue;
        }
        QTextEdit::ExtraSelection sel;
        sel.format.setBackground(QColor(220, 220, 220));
        sel.format.setProperty(QTextFormat::FullWidthSelection, true);
        sel.cursor = QTextCursor(b);
        sel.cursor.setPosition(b.position());
        sel.cursor.setPosition(b.position() + b.length() - 1, QTextCursor::KeepAnchor);
        m_parsedGrey << sel;
    }
    ui->plainTextEdit->setExtraSelections(m_parsedGrey);
}

// 点中解析表格的某一行时，在报文收发窗口把对应字节标绿
void MainWindow::highlightLogByteForParseRow(int row)
{
    // 先恢复成只有灰色标记的状态
    ui->plainTextEdit->setExtraSelections(m_parsedGrey);

    QTableWidgetItem *item = ui->parseTable->item(row, 0);
    if (!item) {
        return;
    }
    const int absByte = item->data(Qt::UserRole).toInt();
    int byteLen = item->data(Qt::UserRole + 1).toInt();
    if (absByte < 0 || byteLen <= 0) {
        return; // 非字节明细行（标题、字段汇总、空行等）
    }

    for (int s = 0; s < m_parsedSources.size(); ++s) {
        const LogByteSource &src = m_parsedSources.at(s);
        if (absByte < src.startByte || absByte >= src.startByte + src.byteCount) {
            continue;
        }

        QTextBlock logBlock = ui->plainTextEdit->document()->findBlockByNumber(src.blockNumber);
        if (!logBlock.isValid()) {
            return;
        }
        const QString logText = logBlock.text();
        const int colon = logText.indexOf(QChar(0xFF1A));
        if (colon < 0) {
            return;
        }

        const int byteIndex = absByte - src.startByte;
        const int tokenPos = hexTokenCharPos(logText, colon, byteIndex);
        if (tokenPos < 0) {
            return;
        }

        // 高亮范围不能超出该日志行已有的字节
        const int maxBytes = src.byteCount - byteIndex;
        if (byteLen > maxBytes) {
            byteLen = maxBytes;
        }
        if (byteLen <= 0) {
            return;
        }

        const int startPos = logBlock.position() + tokenPos;
        const int endPos = startPos + 3 * (byteLen - 1) + 2;
        const int maxPos = logBlock.position() + logBlock.length() - 1;

        QTextEdit::ExtraSelection green;
        green.format.setBackground(QColor(110, 220, 110));
        green.cursor = QTextCursor(logBlock);
        green.cursor.setPosition(startPos);
        green.cursor.setPosition(qMin(endPos, maxPos), QTextCursor::KeepAnchor);

        QList<QTextEdit::ExtraSelection> all = m_parsedGrey;
        all << green;
        ui->plainTextEdit->setExtraSelections(all);

        // 滚动到该字节（不建立选区，避免盖住绿色）
        QTextCursor scrollCursor(logBlock);
        scrollCursor.setPosition(startPos);
        ui->plainTextEdit->setTextCursor(scrollCursor);
        return;
    }
}
