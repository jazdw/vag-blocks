#include <QtGui/QApplication>
#include "mainwindow.h"

int main(int argc, char *argv[])
{
    QApplication a(argc, argv);

    QCoreApplication::setOrganizationName("JaredWiltshire");
    QCoreApplication::setOrganizationDomain("jazdw.net");
    QCoreApplication::setApplicationName("VAG Blocks");

    MainWindow w;
    w.show();
    
    return a.exec();
}
