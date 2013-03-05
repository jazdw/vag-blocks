#include "canframe.h"

canFrame::canFrame(int canID, int length, const QByteArray &data, QObject *parent) :
    QObject(parent),
    canID(canID), length(length), data(data)
{
}
