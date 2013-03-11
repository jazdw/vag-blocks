/*
Copyright 2013 Jared Wiltshire

This file is part of VAG Blocks.

VAG Blocks is free software: you can redistribute it and/or modify
it under the terms of the GNU General Public License as published by
the Free Software Foundation, either version 3 of the License, or
(at your option) any later version.

VAG Blocks is distributed in the hope that it will be useful,
but WITHOUT ANY WARRANTY; without even the implied warranty of
MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
GNU General Public License for more details.

You should have received a copy of the GNU General Public License
along with VAG Blocks.  If not, see <http://www.gnu.org/licenses/>.
*/

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
