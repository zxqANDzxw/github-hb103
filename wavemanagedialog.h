#ifndef WAVEMANAGEDIALOG_H
#define WAVEMANAGEDIALOG_H

#include <QDialog>
#include "iprotocol.h" // 提供wavefileinfo的引用


class TcpClient;
class IProtocol;
class QPlainTextEdit;


namespace Ui {
class WaveManageDialog;
}

class WaveManageDialog : public QDialog
{
    Q_OBJECT

public:
    explicit WaveManageDialog(QWidget *parent,
                              TcpClient *tcp,
                              IProtocol *proto,
                              QPlainTextEdit *logEdit);
    ~WaveManageDialog();

private slots:
    void on_pushButton_clicked(); // 召唤录波列表按钮槽
    // 新增：接收规约层的录波列表信号
    void onWaveListReceived(const QList<WaveFileInfo>& waveList);

private:
    Ui::WaveManageDialog *ui;
    // 保存主窗口传入的指针，直接复用
    TcpClient *m_tcp;
    IProtocol *m_proto;
    QPlainTextEdit *m_logEdit;
};

#endif // WAVEMANAGEDIALOG_H
