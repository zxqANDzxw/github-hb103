#include "mainwindow.h"
#include "ui_mainwindow.h"
#include "adddevicedialog.h"
#include "wavemanagedialog.h"
#include <QMessageBox>
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
    ui->splitter_2->setStretchFactor(1, 1);  // 报文解析窗口

    // 1. 初始化 TCP
    m_tcp = new TcpClient(this);

    // 2. 初始化 华北103
    m_protocol = new NorthChina103(this);

    int gitlalala;

 //tcp连接成功后 关联执行
  /*  connect(m_tcp, &TcpClient::sigConnected, this, [=](){
    ui->plainTextEdit->appendPlainText("==== TCP设备链路建立成功 ====");
    }, Qt::UniqueConnection);
  */
    // 连接成功后启动轮询，不用手动管理定时器
    connect(m_tcp, &TcpClient::sigConnected, this, [=]{
        m_protocol->startPoll();
    });


 //tcp收到数据后数据帧处理，只喂给规约层
    connect(m_tcp, &TcpClient::sigRecvRawData, this, [=](const QByteArray& data){
        printRecvLog(data, ui->plainTextEdit);
        m_protocol->feedRawData(data);
    });

 // 规约层请求发送：统一调用TCP
    connect(m_protocol, &IProtocol::requestSend, this, [=](const QByteArray& frame){
        m_tcp->sendRawData(frame);
        printSendLog(frame, ui->plainTextEdit);
    });


  /*  connect(m_tcp, &TcpClient::sigRecvRawData, this, [=](const QByteArray &data)
    {
        // 1. 将上一个数据包尾部不完整数据 追加新数据，拼接完整数据流
        QByteArray buf = m_protocol->getRecvBuf();
        buf.append(data);

        int usedBytes = 0;
        // 2. 解析全部完整帧 frames里包含拆解好的多帧数据 包含长帧 和 固定帧。
        //同时把裁剪掉buf中已经用过的数据，做成新的newbuf
        QList<QByteArray> frames = m_protocol->splitRawFrameWithCache(buf, usedBytes);
        QByteArray newBuf = buf.mid(usedBytes);

        m_protocol->setRecvBuf(newBuf);//把没用完的数据帧回填给northChina103对象。

        // 防溢出保护
        if(m_protocol->getRecvBuf().size() > 1024)
        {
            m_protocol->clearRecvBuf();
            qDebug() << "接收缓存超限，清空脏数据";
        }

        // 处理frames当中的数据帧
        foreach (auto frame, frames)
        {
            printRecvLog(frame, ui->plainTextEdit);

            bool acd = false;
            if (frame.size() == 5 && (uchar)frame[0] == 0x10 && (uchar)frame[4] == 0x16) {
                uchar control = frame[1];
                acd = (control & 0x20) != 0;
            }
            if(frame.size() >= 5 && (uchar)frame[0] == 0x68 && (uchar)frame[3] == 0x68) {
                uchar control = frame[4];
                acd = (control & 0x20) != 0;
            }

            if (acd) {
                QByteArray f = m_protocol->buildFrame({{"type","召唤一级数据"},{"addr",1}});
                m_tcp->sendRawData(f);

               // 【发送】召唤一级数据
               printSendLog(f,ui->plainTextEdit);
            }
        }
    });

        // 定时 1秒 发一次召唤二级数据
        QTimer *timerPoll = new QTimer(this);
        connect(timerPoll, &QTimer::timeout, this, [=](){
        if (!m_tcp->isConnected()) return;

        // 定时发二级数据
        QByteArray f = m_protocol->buildFrame({{"type","召唤二级数据"},{"addr",1}});
        m_tcp->sendRawData(f);
        printSendLog(f,ui->plainTextEdit);

    });
    timerPoll->start(600000); // 1秒一次
    */
}








MainWindow::~MainWindow()
{
    delete ui;
}
//右击菜单操作
void MainWindow::on_treeWidget_customContextMenuRequested(const QPoint &pos)
{
    // 获得你右键点的那台设备
    QTreeWidgetItem *item = ui->treeWidget->itemAt(pos);

    if (item)
       {
           // =========================
           // 情况1：右键点到了【设备】
           // =========================
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
           if(!act) return;

           // 这里写你原来的命令处理
           QString cmd = act->text();
           if(cmd == "连接设备"){
               // 1. 获取当前选中的设备项
               QTreeWidgetItem *item = ui->treeWidget->currentItem();
               if (!item) return;

               // 2. 读取设备配置信息（之前setData存进去的）
               QString ip = item->data(0, Qt::UserRole + 1).toString();
               int port = item->data(0, Qt::UserRole + 2).toInt();

               // 3. 断开之前的连接（如果有）
               if (m_tcp->isConnected()) {
                   m_tcp->disconnectTcp();
               }

               // 4. 连接新设备
               bool ok = m_tcp->connectTcp(ip, port);
               if (!ok) {

                    ui->plainTextEdit->appendPlainText(QString("=== 连接设备失败：%1:%2 ===").arg(ip).arg(port));
               }

           }
           else if(cmd == "总召唤"){
               if (!m_tcp->isConnected()) {
                       ui->plainTextEdit->appendPlainText("请先连接设备！");
                       return;
                   }


               // 2. 调用规约类构建帧（核心！消除重复代码）
                  QVariantMap param;
                  param["type"] = "总召唤";
                  // （可选）如果以后要支持动态设备地址，可以在这里传 param["addr"] = 设备地址
                  QByteArray frame = m_protocol->buildFrame(param);

                  // 3. 检查帧构建结果
                  if (frame.isEmpty()) {
                      ui->plainTextEdit->appendPlainText("错误：构建总召唤帧失败！");
                      return;
                  }

                   // 发送
                   m_tcp->sendRawData(frame);
                   // 日志界面显示
                    printSendLog(frame,ui->plainTextEdit);
           }

           else if(cmd == "复位通信单元C_RCU_NA_3"){
               if (!m_tcp->isConnected()) {
                       ui->plainTextEdit->appendPlainText("请先连接设备！");
                       return;
                   }
                   // 构建并发送复位通信单元帧
                   QVariantMap param;
                   param["type"] = "复位通信单元";
                   param["addr"] = 1; // 可根据设备动态调整
                   QByteArray frame = m_protocol->buildFrame(param);
                   if (frame.isEmpty()) {
                       ui->plainTextEdit->appendPlainText("错误：构建复位通信单元帧失败！");
                       return;
                   }
                   m_tcp->sendRawData(frame);
                   printSendLog(frame,ui->plainTextEdit);
                   //m_logger->writeLog(MsgDirection::Send, hex);
           }

           else if(cmd == "复位帧计数位C_RFB_NA_3"){
               if (!m_tcp->isConnected()) {
                       ui->plainTextEdit->appendPlainText("请先连接设备！");
                       return;
                   }
                   // 构建并发送复位帧计数位帧
                   QVariantMap param;
                   param["type"] = "复位帧计数位";
                   param["addr"] = 1;
                   QByteArray frame = m_protocol->buildFrame(param);
                   if (frame.isEmpty()) {
                       ui->plainTextEdit->appendPlainText("错误：构建复位帧计数位帧失败！");
                       return;
                   }
                   m_tcp->sendRawData(frame);
                   printSendLog(frame,ui->plainTextEdit);

               }


           else if(cmd == "装置对时C_SYN_TA_3"){

           }

           else if(cmd == "召唤二级数据C_PL2_NA_3"){
               if (!m_tcp->isConnected()) {
                       ui->plainTextEdit->appendPlainText("请先连接设备！");
                       return;
                   }


               // 2. 调用规约类构建帧（核心！消除重复代码）
                  QVariantMap param;
                  param["type"] = "召唤二级数据";

                  QByteArray frame = m_protocol->buildFrame(param);

                  // 3. 检查帧构建结果
                  if (frame.isEmpty()) {
                      ui->plainTextEdit->appendPlainText("错误：构建总召唤帧失败！");
                      return;
                  }

                   // 发送
                   m_tcp->sendRawData(frame);

                   // 日志 + 界面显示
                   printSendLog(frame,ui->plainTextEdit);
           }

           else if(cmd == "召唤一级数据C_PL1_NA_3"){
               if (!m_tcp->isConnected()) {
                       ui->plainTextEdit->appendPlainText("请先连接设备！");
                       return;
                   }


               // 2. 调用规约类构建帧（核心！消除重复代码）
                  QVariantMap param;
                  param["type"] = "召唤一级数据";

                  QByteArray frame = m_protocol->buildFrame(param);

                  // 3. 检查帧构建结果
                  if (frame.isEmpty()) {
                      ui->plainTextEdit->appendPlainText("错误：构建总召唤帧失败！");
                      return;
                  }

                   // 发送
                   m_tcp->sendRawData(frame);

                   // 日志 + 界面显示
                    printSendLog(frame,ui->plainTextEdit);
           }
           else if(cmd=="录波操作"){


               WaveManageDialog WaveDialog(this,m_tcp,m_protocol,ui->plainTextEdit);


               // 4. 用户点了确定，就更新设备信息
               if (WaveDialog.exec() == QDialog::Accepted){}

           }
           else if(cmd == "编辑设备"){
               // 1. 获取当前选中的设备项
                   QTreeWidgetItem *item = ui->treeWidget->currentItem();
                   if (!item) return;

                   // 2. 读取设备现有的配置信息（之前setData存进去的）
                   QString oldName = item->text(0);
                   QString oldIp = item->data(0, Qt::UserRole + 1).toString();
                   int oldPort = item->data(0, Qt::UserRole + 2).toInt();

                   // 3. 复用AddDeviceDialog，把旧配置填进去
                   AddDeviceDialog dialog(this);
                   dialog.setWindowTitle("编辑设备"); // 改一下标题，区分添加和编辑
                   dialog.setDeviceName(oldName);
                   dialog.setIpAddress(oldIp);
                   dialog.setPort(oldPort);

                   // 4. 用户点了确定，就更新设备信息
                   if (dialog.exec() == QDialog::Accepted)
                   {
                       QString newName = dialog.deviceName();
                       QString newIp = dialog.ipAddress();
                       int newPort = dialog.port();
                       if (newPort < 1 || newPort > 65535) {
                          QMessageBox::warning(this, "错误", "端口号必须在1-65535之间！");
                                return;
                            }
                       // 更新显示文本
                       item->setText(0, newName.isEmpty() ? oldName : newName);
                       // 更新IP和端口数据
                       item->setData(0, Qt::UserRole + 1, newIp);
                       item->setData(0, Qt::UserRole + 2, newPort);
                   }

           }else if(cmd=="删除设备"){
               // 获取当前选中的设备项
                  QTreeWidgetItem *item = ui->treeWidget->currentItem();
                  if (!item) return;

                  // 弹出确认框，防止误删
                  QMessageBox::StandardButton reply;
                  reply = QMessageBox::question(this, "确认删除",
                                               QString("确定要删除设备「%1」吗？").arg(item->text(0)),
                                               QMessageBox::Yes | QMessageBox::No);
                  if (reply == QMessageBox::Yes) {
                      delete item; // 直接删除列表项
                  }
           }
       }
       else
       {
           // =========================
           // 情况2：右键点到【空白处】
           // =========================
           QMenu menu;
           QAction *addDevice = menu.addAction("添加设备");
           QAction *act = menu.exec(ui->treeWidget->mapToGlobal(pos));

           if(act == addDevice)
           {
               AddDeviceDialog dialog(this);
               dialog.setWindowTitle("添加设备");
               if (dialog.exec() == QDialog::Accepted)
               {
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
           }
        }
    }
//选中解析功能
void MainWindow::on_plainTextEdit_cursorPositionChanged()
        {
            // 获取当前行号
            int row = ui->plainTextEdit->textCursor().blockNumber();

            if (row < 0 ) {
                ui->textEdit->clear();
                return;
            }

            if (row > 0 ) {
                ui->textEdit->append("=== 华北103 解析结果 ===");
                return;
            }

        }
