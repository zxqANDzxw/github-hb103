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
        ui->tableWidget->setHorizontalHeaderLabels({"故障时间", "录波文件名", "文件大小"});
        ui->tableWidget->horizontalHeader()->setStretchLastSection(true);

        // 连接规约层的录波列表信号
        connect(m_proto, &IProtocol::waveListReceived,
                this, &WaveManageDialog::onWaveListReceived);

        // 补充：连接错误提示信号
        connect(m_proto, &IProtocol::waveListError, this, [=](const QString &msg)
        {m_logEdit->appendPlainText(QString("【错误】%1").arg(msg));});


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
        ui->tableWidget->setItem(row, 0, new QTableWidgetItem(info.faultTime.toString("yyyy-MM-dd HH:mm:ss.zzz")));
        ui->tableWidget->setItem(row, 1, new QTableWidgetItem(info.fileName));
        ui->tableWidget->setItem(row, 2, new QTableWidgetItem(QString::number(info.fileSize)));
    }
}
