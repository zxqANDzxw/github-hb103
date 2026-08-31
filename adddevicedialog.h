#ifndef ADDDEVICEDIALOG_H
#define ADDDEVICEDIALOG_H

#include <QDialog>

namespace Ui {
class AddDeviceDialog;
}

class AddDeviceDialog : public QDialog
{
    Q_OBJECT

public:
    explicit AddDeviceDialog(QWidget *parent = nullptr);
    ~AddDeviceDialog();

    // 给主窗口调用的读取配置的接口
    QString deviceName() const;
    QString ipAddress() const;
    int port() const;
    // 在AddDeviceDialog类里，public部分加上这三个函数
    void setDeviceName(const QString &name);
    void setIpAddress(const QString &ip);
    void setPort(int port);

private:
    Ui::AddDeviceDialog *ui;
};

#endif // ADDDEVICEDIALOG_H
