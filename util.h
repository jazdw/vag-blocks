#ifndef UTIL_H
#define UTIL_H

#include <QString>

QString toHex(int num, int places = 2);
QString toHex(unsigned int num, int places = 2);
QString intToBinary(int num, int places = 8);
QString uintToBinary(unsigned int num, int places = 8);
QString doubleToStr(double num, int prec = 2);
int fromHex(QString str);

enum {
    stdLog = 0x01,
    rxTxLog = 0x02,
    serialConfigLog = 0x04,
    keepAliveLog = 0x08,
    responseErrorLog = 0x10,
    debugMsgLog = 0x20
};

#endif // UTIL_H
