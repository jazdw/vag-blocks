#ifndef MONITOR_H
#define MONITOR_H

#include <QObject>
#include <QFile>
#include <QTextStream>

#include "elm327.h"

class monitor : public QObject
{
    Q_OBJECT
public:
    explicit monitor(elm327* elm, QObject *parent = 0);
    
signals:
    void done();
public slots:
    void start();
    void stop();
private:
    elm327* elm;

    QString filename;
    QFile* outFile;
    QTextStream out;

    bool monitoring;
};

#endif // MONITOR_H
