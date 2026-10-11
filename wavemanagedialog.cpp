#include "wavemanagedialog.h"
#include "ui_wavemanagedialog.h"

// 引入依赖类的完整定义
#include "tcpclient.h"
#include "northchina103.h"
#include <QPlainTextEdit>
#include <QDateTime>
#include <QDir>

WaveManageDialog::WaveManageDialog(QWidget *parent,TcpClient *tcp,IProtocol *proto,QPlainTextEdit *logEdit) :
    QDialog(parent),
    ui(new Ui::WaveManageDialog),
    m_tcp(tcp),
    m_proto(proto),
    m_logEdit(logEdit)
{
    ui->setupUi(this);
    setWindowTitle("录波文件管理");

    // 初始化默认时间
        QDateTime now = QDateTime::currentDateTime();
        ui->dtStart->setDateTime(now.addDays(-1));
        ui->dtEnd->setDateTime(now);

        // 初始化表格
        ui->tableWidget->setColumnCount(3);
        ui->tableWidget->setHorizontalHeaderLabels({"故障时间", "录波文件名", "操作"});
        ui->tableWidget->horizontalHeader()->setStretchLastSection(true);

        // 连接规约层的录波列表信号
        connect(m_proto, &IProtocol::waveListReceived,
                this, &WaveManageDialog::onWaveListReceived);

        // 补充：连接错误提示信号
        connect(m_proto, &IProtocol::waveListError, this, [=](const QString &msg)
        {m_logEdit->appendPlainText(QString("【错误】%1").arg(msg));});


        // 3. 下载进度
        connect(m_proto, &IProtocol::waveDownloadProgress, this, [=](quint32 recv, quint32 total){
            if (total > 0) {
                m_logEdit->appendPlainText(QString("【下载进度】%1 / %2 字节").arg(recv).arg(total));
            } else {
                m_logEdit->appendPlainText(QString("【下载进度】已接收 %1 字节").arg(recv));
            }
        });

            connect(m_proto, &IProtocol::waveDownloadFinished,
                    this, [=](const QString &name, const QByteArray &hdr, const QByteArray &cfg, const QByteArray &dat){
                m_logEdit->appendPlainText(QString("【下载完成】%1，总数据 %2 字节").arg(name).arg(hdr.size()+cfg.size()+dat.size()));

                // 更新表格样式（原有逻辑保留）
                if (m_currentDownloadRow >= 0) {
                    QTableWidgetItem *item = ui->tableWidget->item(m_currentDownloadRow, 1);
                    if (item) {
                        item->setForeground(QBrush(Qt::darkGreen));
                        QFont font = item->font();
                        font.setBold(false);
                        item->setFont(font);
                        item->setData(Qt::UserRole, true);
                    }
                    // 恢复按钮状态
                    QWidget *btnWidget = qobject_cast<QWidget*>(ui->tableWidget->cellWidget(m_currentDownloadRow, 2));
                    if (btnWidget) {
                        QPushButton *btn = btnWidget->findChild<QPushButton*>("btnDownload");
                        if (btn) {
                            btn->setEnabled(true);
                            btn->setText("已下载");
                        }
                    }
                }

                // ========== 文件保存逻辑（带错误检测） ==========
                QString saveDir = QCoreApplication::applicationDirPath() + "/wave_files";
                QDir dir;
                if (!dir.exists(saveDir)) {
                    bool mkOk = dir.mkpath(saveDir);
                    if (!mkOk) {
                        m_logEdit->appendPlainText(QString("【保存失败】无法创建目录：%1").arg(saveDir));
                        m_currentDownloadRow = -1;
                        return;
                    }
                    m_logEdit->appendPlainText(QString("【保存】自动创建目录：%1").arg(saveDir));
                }

                // 通用保存函数
                auto saveFile = [&](const QString &suffix, const QByteArray &data) -> bool {
                    QString fullPath = saveDir + "/" + name + suffix;
                    QFile file(fullPath);
                    if (!file.open(QIODevice::WriteOnly)) {
                        m_logEdit->appendPlainText(QString("【保存失败】%1 写入失败：%2").arg(fullPath, file.errorString()));
                        return false;
                    }
                    file.write(data);
                    file.close();
                    return true;
                };

                bool hdrOk = saveFile(".hdr", hdr);
                bool cfgOk = saveFile(".cfg", cfg);
                bool datOk = saveFile(".dat", dat);

                if (hdrOk && cfgOk && datOk) {
                    m_logEdit->appendPlainText("【保存成功】文件已保存到 wave_files 目录");
                }

                m_currentDownloadRow = -1;
            });



}

WaveManageDialog::~WaveManageDialog()
{
    delete ui;
}
// 按钮点击：直接复用规约层能力
void WaveManageDialog::on_pushButton_clicked()
{

    // 只调用规约层业务接口，统一走requestSend发送、统一打日志、统一管理FCB
    m_proto->callWaveFileList(ui->dtStart->dateTime(), ui->dtEnd->dateTime());
    m_logEdit->appendPlainText("【发送】召唤录波文件列表");
}

// 刷新录波列表表格
void WaveManageDialog::onWaveListReceived(const QList<WaveFileInfo> &waveList)
{

    qDebug() << "[调试] 界面收到列表，数量:" << waveList.size(); // 加这行
    // 先清空原有内容
       ui->tableWidget->setRowCount(0);


    for (const auto& info : waveList)
    {
        int row = ui->tableWidget->rowCount();
        ui->tableWidget->insertRow(row);

        // 第1列：故障时间
        ui->tableWidget->setItem(row, 0,
            new QTableWidgetItem(info.faultTime.toString("yyyy-MM-dd HH:mm:ss.zzz")));

        // 第2列：录波文件名（默认未下载：蓝色 + 加粗）
        QTableWidgetItem *nameItem = new QTableWidgetItem(info.fileName);
        nameItem->setForeground(QBrush(Qt::blue));
        QFont nameFont = nameItem->font();
        nameFont.setBold(true);
        nameItem->setFont(nameFont);
        nameItem->setData(Qt::UserRole, false); // false=未下载，true=已下载
        ui->tableWidget->setItem(row, 1, nameItem);

        // 第3列：操作按钮组（下载、分析、删除）
        QWidget *btnWidget = new QWidget();
        QHBoxLayout *btnLayout = new QHBoxLayout(btnWidget);
        btnLayout->setContentsMargins(4, 2, 4, 2);
        btnLayout->setSpacing(6);

        QPushButton *btnDownload = new QPushButton("下载", btnWidget);
        QPushButton *btnAnalyze  = new QPushButton("分析", btnWidget);
        QPushButton *btnDelete   = new QPushButton("删除", btnWidget);

        // 按钮统一尺寸
        btnDownload->setFixedWidth(60);
        btnAnalyze->setFixedWidth(60);
        btnDelete->setFixedWidth(60);

        btnLayout->addWidget(btnDownload);
        btnLayout->addWidget(btnAnalyze);
        btnLayout->addWidget(btnDelete);
        btnLayout->addStretch();

        ui->tableWidget->setCellWidget(row, 2, btnWidget);

        // 绑定按钮点击事件
        connect(btnDownload, &QPushButton::clicked, this, [=](){
            onDownloadFile(info.fileName, row);
        });
        connect(btnAnalyze, &QPushButton::clicked, this, [=](){
            onAnalyzeFile(info.fileName, row);
        });
        connect(btnDelete, &QPushButton::clicked, this, [=](){
            onDeleteFile(info.fileName, row);
        });
    }
}

void WaveManageDialog::onDownloadFile(const QString &fileName, int row)
{
    m_logEdit->appendPlainText(QString("【下载】开始下载录波文件：%1").arg(fileName));
        // 调用规约层下载接口（后续补充完整下载逻辑）
       // m_proto->downloadWaveFile(fileName);

        // 模拟下载完成后更新样式（实际项目放到下载完成回调里）
        QTableWidgetItem *item = ui->tableWidget->item(row, 1);
        if (item) {
            item->setForeground(QBrush(Qt::darkGreen));
            QFont font = item->font();
            font.setBold(false);
            item->setFont(font);
            item->setData(Qt::UserRole, true);
        }

}

void WaveManageDialog::onAnalyzeFile(const QString &fileName, int row)
{
    m_logEdit->appendPlainText(QString("【分析】打开录波分析：%1").arg(fileName));
    // 后续接入录波分析模块
}

void WaveManageDialog::onDeleteFile(const QString &fileName, int row)
{
    m_logEdit->appendPlainText(QString("【删除】移除录波条目：%1").arg(fileName));
    ui->tableWidget->removeRow(row);
}
