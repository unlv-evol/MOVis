#include "headers/mainwindow.h"

#include <QApplication>

int main(int argc, char *argv[])
{
    QApplication app(argc, argv);
    app.setWindowIcon(QIcon("assets/icons/main.ico"));

    // These two lines define where QSettings will persist your data
    QCoreApplication::setOrganizationName("Evol");
    QCoreApplication::setApplicationName("MOVis");

    MainWindow w;
    w.show();
    w.setWindowIcon(QIcon("assets/icons/main.ico"));
    return app.exec();
}
