#include "util.h"

QString toHex(int num, int places)
{
    return QString("%1").arg(num, places, 16, QChar('0')).toUpper();
}

QString toHex(unsigned int num, int places)
{
    return QString("%1").arg(num, places, 16, QChar('0')).toUpper();
}

QString intToBinary(int num, int places)
{
    return QString("%1").arg(num, places, 2, QChar('0'));
}

QString uintToBinary(unsigned int num, int places)
{
    return QString("%1").arg(num, places, 2, QChar('0'));
}

QString doubleToStr(double num, int prec)
{
    return QString("%1").arg(num, 0, 'f', prec);
}

int fromHex(QString str)
{
    bool ok;
    return str.toInt(&ok, 16);
}
