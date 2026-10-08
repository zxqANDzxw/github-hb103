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
        ui->tableWidget->setHorizontalHeaderLabels({"故障时间", "录波文件名", "操作"});
        ui->tableWidget->horizontalHeader()->setStretchLastSection(true);

        // 连接规约层的录波列表信号
        connect(m_proto, &IProtocol::waveListReceived,
                this, &WaveManageDialog::onWaveListReceived);

        // 补充：连接错误提示信号
        connect(m_proto, &IProtocol::waveListError, this, [=](const QString &msg)
        {m_logEdit->appendPlainText(QString("【错误】%1").arg(msg));});

        // 连接信号 + 打印连接结果
            bool ok = connect(m_proto, &IProtocol::waveListReceived, this, &WaveManageDialog::onWaveListReceived);
            qDebug() << "[对话框] 信号连接结果：" << ok;



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
