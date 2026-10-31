#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "adddevicedialog.h"
#include "wavemanagedialog.h"
#include <QMessageBox>
#include <QMenu>
#include <QAction>
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

    // 初始化录波对话框：非模态、父窗口绑定、只创建一次
    m_waveDialog = new WaveManageDialog(this, m_tcp, m_protocol, ui->plainTextEdit);
    m_waveDialog->setAttribute(Qt::WA_DeleteOnClose, false); // 关闭不销毁，反复复用

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
    WaveManageDialog *dlg = new WaveManageDialog(this, m_tcp, m_protocol, ui->plainTextEdit);
    dlg->setAttribute(Qt::WA_DeleteOnClose);  // 关闭窗口自动销毁内存
    dlg->setWindowModality(Qt::NonModal);    // 显式指定非模态
    dlg->show();
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

// 选中解析功能（保持原实现）
void MainWindow::on_plainTextEdit_cursorPositionChanged()
{
    // 获取当前行号
    int row = ui->plainTextEdit->textCursor().blockNumber();

    if (row < 0) {
        ui->textEdit->clear();
        return;
    }

    if (row > 0) {
        ui->textEdit->append("=== 华北103 解析结果 ===");
        return;
    }
}
