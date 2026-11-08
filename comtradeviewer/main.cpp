#include <QApplication>
#include <QStringList>

#include "viewerwindow.h"

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);

    ViewerWindow window;
    const QStringList arguments = app.arguments();
    if (arguments.size() > 1) {
        window.loadFile(arguments.at(1));
    }
    window.show();
    return app.exec();
}
