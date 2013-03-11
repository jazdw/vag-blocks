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

#ifndef ELM327_H
#define ELM327_H

#include <QObject>
#include <QMutex>
#include <QWaitCondition>
#include <QStringList>

#include <serialport.h>
using namespace QtAddOn::SerialPort;

#include "canframe.h"
#include "serialsettings.h"
#include "util.h"

enum responses {
    TIMEOUT_ERROR = 0x01,
    NO_PROMPT_ERROR = 0x02,
    OK_RESPONSE = 0x04,
    STOPPED_RESPONSE = 0x08,
    UNKNOWN_RESPONSE = 0x10,
    AT_RESPONSE = 0x20,
    NO_DATA_RESPONSE = 0x40,
    PROCESSING_ERROR = 0x80,
    CAN_ERROR = 0x100
};

class elm327 : public QObject
{
    Q_OBJECT
public:
    explicit elm327(QObject *parent = 0);
    ~elm327();
    QList<canFrame*>* getResponseCAN(int &status);
    QString getResponseStr(int &status);
    bool getResponseStatus(int &status);
    QString getLine(int timeout = 1100, bool wait = true);
    void setSerialParams(const serialSettings &in);
    bool getPortOpen();
signals:
    void log(const QString &txt, int logLevel = stdLog, bool flush = false);
    void portOpened(bool status);
    void portClosed();
public slots:
    void closePort();
    void openPort();
    void write(const QByteArray &data);
    void write(const QString &txt);
    void setSendCanID(int id);
    void setRecvCanID(int id);
private slots:
    void constructLine();
private:
    SerialPort* port;
    serialSettings settings;
    QStringList bufferedLines;
    QMutex* protectLines;
    QWaitCondition linesAvailable;
    canFrame *hexToCF(QString &input);

    int sendCanID;
    int recvCanID;
    bool portOpen;
};

#endif // ELM327_H
