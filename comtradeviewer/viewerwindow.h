#ifndef VIEWERWINDOW_H
#define VIEWERWINDOW_H

#include <QMainWindow>

#include "comtrade.h"

class QListWidget;
class QListWidgetItem;
class QLabel;
class WaveformView;

class ViewerWindow : public QMainWindow
{
    Q_OBJECT
public:
    explicit ViewerWindow(QWidget *parent = nullptr);

    // 打开一个 COMTRADE 的 .cfg（同目录同名 .dat 自动加载）
    bool loadFile(const QString &cfgPath);

private slots:
    void openFileDialog();
    void onItemChanged(QListWidgetItem *item);

private:
    void populateChannels();

    QListWidget *m_channelList;
    WaveformView *m_waveView;
    QLabel *m_infoLabel;
    CtData m_data;
};

#endif // VIEWERWINDOW_H
