#include "mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{

    // 注册自定义类型，必须放在QApplication构造之前
    qRegisterMetaType<WaveFileInfo>("WaveFileInfo");
    qRegisterMetaType<QList<WaveFileInfo>>("QList<WaveFileInfo>");

    QApplication a(argc, argv);
    MainWindow w;
    w.show();
    return a.exec();
}
