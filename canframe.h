#ifndef CANFRAME_H
#define CANFRAME_H

#include <QObject>
#include <QByteArray>

class canFrame : public QObject
{
    Q_OBJECT
public:
    explicit canFrame(int canID, int length, const QByteArray &data, QObject *parent = 0);
    int canID;
    int length;
    QByteArray data;
signals:
    
public slots:
    
};

#endif // CANFRAME_H
