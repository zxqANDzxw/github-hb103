#include "wavemanagedialog.h"
#include "ui_wavemanagedialog.h"

// 引入依赖类的完整定义
#include "tcpclient.h"
#include "northchina103.h"
#include <QPlainTextEdit>
#include <QDateTime>

WaveManageDialog::WaveManageDialog(QWidget *parent,TcpClient *tcp,IProtocol *proto,QPlainTextEdit *logEdit) :
    QDialog(parent),
    ui(new Ui::WaveManageDialog),
    m_tcp(tcp),    // 保存主窗口TCP对象指针
    m_proto(proto),// 保存主窗口规约对象指针
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
        ui->tableWidget->setHorizontalHeaderLabels({"故障时间", "录波文件名", "文件大小"});
        ui->tableWidget->horizontalHeader()->setStretchLastSection(true);

        // 连接规约层的录波列表信号
        connect(m_proto, &IProtocol::waveListReceived,
                this, &WaveManageDialog::onWaveListReceived);
}

WaveManageDialog::~WaveManageDialog()
{
    delete ui;
}
// 按钮点击：直接复用规约层能力
void WaveManageDialog::on_pushButton_clicked()
{

    m_proto->callWaveFileList(ui->dtStart->dateTime(), ui->dtEnd->dateTime());
    QVariantMap param;
        param["type"] = "召唤录波列表_按时间";
        param["addr"] = 0x01;
        // 补上两个必填时间参数，从界面控件取值
        param["startTime"] = ui->dtStart->dateTime();
        param["endTime"] = ui->dtEnd->dateTime();

        QByteArray frame = m_proto->buildFrame(param);
        if(frame.isEmpty()) {
            m_logEdit->appendPlainText("【错误】构建录波召唤帧失败");
            return;
        }
        qDebug() << "录波帧内容:" << frame.toHex(' ').toUpper();
        m_tcp->sendRawData(frame);
        m_logEdit->appendPlainText("【发送】召唤录波文件列表");
}

// 刷新录波列表表格
void WaveManageDialog::onWaveListReceived(const QList<WaveFileInfo> &waveList)
{
    for (const auto& info : waveList)
    {
        int row = ui->tableWidget->rowCount();
        ui->tableWidget->insertRow(row);
        ui->tableWidget->setItem(row, 0, new QTableWidgetItem(info.faultTime));
        ui->tableWidget->setItem(row, 1, new QTableWidgetItem(info.fileName));
        ui->tableWidget->setItem(row, 2, new QTableWidgetItem(QString::number(info.fileSize)));
    }
}
