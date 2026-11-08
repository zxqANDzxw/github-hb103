#ifndef WAVEMANAGEDIALOG_H
#define WAVEMANAGEDIALOG_H

#include <QWidget>
#include "iprotocol.h" // 提供wavefileinfo的引用


class TcpClient;
class IProtocol;
class QPlainTextEdit;


namespace Ui {
class WaveManageDialog;
}

// 录波文件管理面板：原为独立对话框，现作为主窗口底部“录波文件管理”页签嵌入
class WaveManageDialog : public QWidget
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

    void onDownloadFile(const QString &fileName, int row);
    void onAnalyzeFile(const QString &fileName, int row);
    void onDeleteFile(const QString &fileName, int row);

private:
    // 列宽按内容自适应（之后仍可拖动表头分隔线手动调整）
    void autoFitColumns();

    Ui::WaveManageDialog *ui;
    // 保存主窗口传入的指针，直接复用
    int m_currentDownloadRow = -1; // 当前下载的行号，-1表示空闲
    TcpClient *m_tcp;
    IProtocol *m_proto;
    QPlainTextEdit *m_logEdit;
};

#endif // WAVEMANAGEDIALOG_H
