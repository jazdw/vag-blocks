#include "monitor.h"

#include <QCoreApplication>

monitor::monitor(elm327 *elm, QObject *parent) :
    QObject(parent),
    elm(elm),
    filename("canLog.txt"),
    outFile(0),
    monitoring(false)
{
}

void monitor::start()
{
    outFile = new QFile(filename, this);
    outFile->open(QIODevice::WriteOnly);
    out.setDevice(outFile);

    QString line;
    monitoring = true;
    int timeoutCount = 0;

    elm->write(QString("ATMA"));

    while (monitoring || timeoutCount >= 5) {
        line = elm->getLine();
        if (line == "") {
            timeoutCount++;
        }

        if (line == ">") {
            monitoring = false;
        }

        out << line << endl;

        //QCoreApplication::processEvents();
    }

    out.flush();
    out.setDevice(0);
    outFile->close();
    delete outFile;
    outFile = 0;

    emit done();
}

void monitor::stop()
{
    monitoring = false;
    elm->write(QString("."));
}
