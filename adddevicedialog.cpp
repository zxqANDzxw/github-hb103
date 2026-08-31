#include "adddevicedialog.h"
#include "ui_adddevicedialog.h"

AddDeviceDialog::AddDeviceDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::AddDeviceDialog)
{
    ui->setupUi(this);
    // 设置端口默认值为2404
     ui->spinBoxPort->setValue(2404);
}

AddDeviceDialog::~AddDeviceDialog()
{
    delete ui;
}

QString AddDeviceDialog::deviceName() const
{
    return ui->lineEditName->text().trimmed();
}

QString AddDeviceDialog::ipAddress() const
{
    return ui->lineEditIp->text().trimmed();
}

int AddDeviceDialog::port() const
{
    return ui->spinBoxPort->value();
}

void AddDeviceDialog::setDeviceName(const QString &name)
{
    ui->lineEditName->setText(name);
}

void AddDeviceDialog::setIpAddress(const QString &ip)
{
    ui->lineEditIp->setText(ip);
}

void AddDeviceDialog::setPort(int port)
{
    ui->spinBoxPort->setValue(port);
}
