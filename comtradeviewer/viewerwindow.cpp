#include "viewerwindow.h"

#include <QAction>
#include <QFileDialog>
#include <QFileInfo>
#include <QHBoxLayout>
#include <QKeySequence>
#include <QLabel>
#include <QListWidget>
#include <QMenu>
#include <QMenuBar>
#include <QMessageBox>
#include <QStatusBar>
#include <QVBoxLayout>

#include "waveformview.h"

namespace {

const int DEFAULT_ANALOG_CHANNELS = 8;
const int DEFAULT_DIGITAL_CHANNELS = 8;
const int CHANNEL_KIND_ANALOG = 0;
const int CHANNEL_KIND_DIGITAL = 1;

} // namespace

ViewerWindow::ViewerWindow(QWidget *parent)
    : QMainWindow(parent)
    , m_channelList(nullptr)
    , m_waveView(nullptr)
    , m_infoLabel(nullptr)
{
    setWindowTitle(QStringLiteral("录波分析 - COMTRADE"));
    resize(1200, 720);

    QWidget *central = new QWidget(this);
    QHBoxLayout *layout = new QHBoxLayout(central);
    layout->setContentsMargins(4, 4, 4, 4);
    layout->setSpacing(4);

    m_channelList = new QListWidget(central);
    m_channelList->setFixedWidth(250);
    m_channelList->setAlternatingRowColors(true);
    m_channelList->setUniformItemSizes(true);

    m_waveView = new WaveformView(central);

    layout->addWidget(m_channelList);
    layout->addWidget(m_waveView, 1);
    setCentralWidget(central);

    connect(m_channelList, &QListWidget::itemChanged, this, &ViewerWindow::onItemChanged);
    connect(m_waveView, &WaveformView::cursorInfo, this, [this](const QString &text){
        if (text.isEmpty()) {
            statusBar()->clearMessage();
        } else {
            statusBar()->showMessage(text);
        }
    });

    QMenu *fileMenu = menuBar()->addMenu(QStringLiteral("文件"));
    QAction *openAction = fileMenu->addAction(QStringLiteral("打开..."));
    openAction->setShortcut(QKeySequence::Open);
    connect(openAction, &QAction::triggered, this, &ViewerWindow::openFileDialog);
    fileMenu->addSeparator();
    QAction *quitAction = fileMenu->addAction(QStringLiteral("退出"));
    connect(quitAction, &QAction::triggered, this, &QWidget::close);

    QMenu *viewMenu = menuBar()->addMenu(QStringLiteral("视图"));
    QAction *resetAction = viewMenu->addAction(QStringLiteral("复位缩放"));
    resetAction->setShortcut(QKeySequence(QStringLiteral("Ctrl+0")));
    connect(resetAction, &QAction::triggered, m_waveView, &WaveformView::resetView);

    m_infoLabel = new QLabel(QStringLiteral("未打开文件"), this);
    statusBar()->addPermanentWidget(m_infoLabel);
    statusBar()->showMessage(QStringLiteral("滚轮缩放时间轴，按住左键拖动平移，双击复位"));
}

bool ViewerWindow::loadFile(const QString &cfgPath)
{
    CtData data;
    QString error;
    if (!ComtradeReader::read(cfgPath, data, &error)) {
        QMessageBox::warning(this, QStringLiteral("打开失败"), error);
        statusBar()->showMessage(error);
        return false;
    }

    m_data = data;
    m_waveView->setData(data);
    populateChannels();

    setWindowTitle(QStringLiteral("录波分析 - ") + QFileInfo(cfgPath).completeBaseName());
    const double averageRate = data.duration > 0.0
                                   ? (data.sampleCount - 1) / data.duration
                                   : 0.0;
    m_infoLabel->setText(
        QStringLiteral("%1 | 起始 %2 | %3 点 | %4 Hz | 时长 %5 s | %6")
            .arg(data.stationName)
            .arg(data.startTime.isValid()
                     ? data.startTime.toString(QStringLiteral("yyyy-MM-dd hh:mm:ss.zzz"))
                     : QStringLiteral("-"))
            .arg(data.sampleCount)
            .arg(averageRate, 0, 'f', 0)
            .arg(data.duration, 0, 'f', 6)
            .arg(data.fileType));
    return true;
}

void ViewerWindow::populateChannels()
{
    m_channelList->blockSignals(true);
    m_channelList->clear();

    const int analogDefault = qMin(m_data.analogs.size(), DEFAULT_ANALOG_CHANNELS);
    const int digitalDefault = qMin(m_data.digitals.size(), DEFAULT_DIGITAL_CHANNELS);

    for (int i = 0; i < m_data.analogs.size(); ++i) {
        const CtAnalogChannel &channel = m_data.analogs.at(i);
        QString text = QStringLiteral("%1  %2").arg(i + 1).arg(channel.id);
        if (!channel.unit.isEmpty()) {
            text += QStringLiteral(" [") + channel.unit + QLatin1Char(']');
        }
        QListWidgetItem *item = new QListWidgetItem(text);
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(i < analogDefault ? Qt::Checked : Qt::Unchecked);
        item->setData(Qt::UserRole, CHANNEL_KIND_ANALOG);
        item->setData(Qt::UserRole + 1, i);
        m_channelList->addItem(item);
        m_waveView->setAnalogVisible(i, i < analogDefault);
    }

    for (int i = 0; i < m_data.digitals.size(); ++i) {
        const CtDigitalChannel &channel = m_data.digitals.at(i);
        QListWidgetItem *item = new QListWidgetItem(QStringLiteral("D%1  %2").arg(i + 1).arg(channel.id));
        item->setFlags(item->flags() | Qt::ItemIsUserCheckable);
        item->setCheckState(i < digitalDefault ? Qt::Checked : Qt::Unchecked);
        item->setData(Qt::UserRole, CHANNEL_KIND_DIGITAL);
        item->setData(Qt::UserRole + 1, i);
        m_channelList->addItem(item);
        m_waveView->setDigitalVisible(i, i < digitalDefault);
    }

    m_channelList->blockSignals(false);
}

void ViewerWindow::openFileDialog()
{
    const QString path = QFileDialog::getOpenFileName(
        this, QStringLiteral("打开 COMTRADE 配置"), QString(),
        QStringLiteral("COMTRADE 配置 (*.cfg *.CFG);;所有文件 (*)"));
    if (!path.isEmpty()) {
        loadFile(path);
    }
}

void ViewerWindow::onItemChanged(QListWidgetItem *item)
{
    if (!item) {
        return;
    }
    const int kind = item->data(Qt::UserRole).toInt();
    const int index = item->data(Qt::UserRole + 1).toInt();
    const bool checked = item->checkState() == Qt::Checked;
    if (kind == CHANNEL_KIND_ANALOG) {
        m_waveView->setAnalogVisible(index, checked);
    } else {
        m_waveView->setDigitalVisible(index, checked);
    }
}
