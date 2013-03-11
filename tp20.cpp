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

#include "tp20.h"
#include "qmath.h"
#include "util.h"
#include <QCoreApplication>

tp20::tp20(elm327* elm, QObject *parent) :
    QObject(parent),
    elm(elm),
    lastResponse(0),
    channelDest(-1),
    txID(0), rxID(0),
    txSeq(0), rxSeq(0),
    elmInitilised(false),
    recvTimeout(-1)
{
    keepAliveTimer.setInterval(500);
    connect(&keepAliveTimer, SIGNAL(timeout()), this, SLOT(sendKeepAlive()));
    keepAliveTimer.start();

    connect(elm, SIGNAL(portOpened(bool)), this, SLOT(initialiseElm(bool)));
    connect(elm, SIGNAL(portClosed()), this, SLOT(portClosed()));
}

int tp20::getChannelDest()
{
    return channelDest;
}

bool tp20::getElmInitialised()
{
    return elmInitilised;
}

void tp20::sendData(const QByteArray &data, int requestedTimeout)
{
    if (channelDest < 0 || data.length() == 0 || data.length() > 65535) {
        return;
    }

    if (recvTimeout != requestedTimeout) {
        emit log("Info: Setting new timeout", debugMsgLog);
        if (!applyRecvTimeout(requestedTimeout)) {
            emit log("Error: Couldn't set timeout", debugMsgLog);
            setChannelClosed();
            return;
        }
    }

    //QCoreApplication::processEvents();
    //QMutexLocker locker(&sendLock);

    quint16 len = data.length();
    QByteArray lenBA;
    lenBA.append(len >> 8);
    lenBA.append(len & 0x0F);

    QByteArray sendData(data);
    sendData.prepend(lenBA);

    int numPackets = qCeil(sendData.length() / 7.0);

    for (int i = 0; i < numPackets; i++) {
        QByteArray packet;
        int bytesLeft = sendData.length() - i*7;

        if (bytesLeft <= 7) { // expecting ACK, last packet 0x1X
            packet.append(0x10 | (txSeq++ & 0x0F));
            packet.append(sendData.mid(i*7, bytesLeft));
            writeToElmBA(packet);
            recvData();
        }
        else if (i % bs == bs-1) { // expecting ACK, more packets to come 0x0X
            packet.append(0x00 | (txSeq++ & 0x0F));
            packet.append(sendData.mid(i*7, 7));
            writeToElmBA(packet);
            // Read in ACK
            if (!getResponseCAN()) {
                emit log("Error: Did not get ACK from TP2.0 device", debugMsgLog);
                return;
            }
            if (lastResponse->length() > 1 || !checkACK()) {
                emit log("Error: Invalid ACK from TP2.0 device", debugMsgLog);
                return;
            }
        }
        else { // more packets to come 0x2X
            packet.append(0x20 | (txSeq++ & 0x0F));
            packet.append(sendData.mid(i*7, 7));
            writeToElmBA(packet);
            // Read in NO DATA
            if (!getResponseStatus(NO_DATA_RESPONSE)) {
                emit log("Error: Got premature response from TP2.0 device", debugMsgLog);
                return;
            }
        }
    }
    return; // should never get to here
}

void tp20::recvData() {
    if (!getResponseCAN()) {
        return;
    }
    if (!checkACK() || lastResponse->length() < 2) {
        return;
    }

    lastResponse->removeFirst(); // remove ACK

    QByteArray* ret = 0;
    bool firstPacket = true;
    bool keepGoing = true;
    quint16 length = 0;
    quint16 bytesReceived = 0;

    int iteration = 0;

    while(keepGoing) {
        iteration++; // debug use only

        if (firstPacket) {
            bytesReceived = 0;
            dataTransFirst dtF;
            if (lastResponse->length() > 0 && lastResponse->at(0)->data.length() > 2) {
                dtF = getAsDTFirst(0);
            }
            else {
                emit log("Error: Incorrectly trying to interpret packet as first packet", debugMsgLog);
                return;
            }
            length = dtF.len;
            length &= 0x7FFF; // mask off MSB, some modules seem to set this for some reason
            lastResponse->at(0)->data.remove(1, 2); // remove the 2 length bytes
            firstPacket = false;

            ret = new QByteArray();
        }

        if (!ret) {
            emit log("Error: TP2.0 return byte array does not exist", debugMsgLog);
            return;
        }

        if (bytesReceived == length) {
            emit log("Warning: Bytes received = packet length, should have returned already", debugMsgLog);
        }

        if (bytesReceived > length) {
            emit log("Warning: Received more bytes than the message length", debugMsgLog);
        }

        if (!checkSeq()) {
            delete ret;
            return;
        }

        int lenTmp = lastResponse->length();
        for (int i = 0; i < lenTmp; i++) {
            dataTrans dt = getAsDT(i);
            if (dt.opcode > 0x3) {
                emit log("Error: Invalid TP2.0 op-code", debugMsgLog);
                delete ret;
                return;
            }
            if (dt.opcode & 0x01) { // last packet
                if (lastResponse->length()-1 > i) {
                    emit log("Error: This is the last TP2.0 packet but there is data following", debugMsgLog);
                    delete ret;
                    return;
                }
                keepGoing = false;
            }

            // add data to ret BA
            ret->append(lastResponse->at(i)->data.mid(1));
            bytesReceived += lastResponse->at(i)->data.length() - 1;

            if (!keepGoing) {
                if (bytesReceived < length) {
                    emit log("Warning: Received less bytes than the TP2.0 message length", debugMsgLog);
                }
                emit response(ret);
                //ret = 0;
            }

            if (!(dt.opcode & 0x02)) { // send ACK
                if (lastResponse->length()-1 > i) {
                    emit log("Error: Was asked to send TP2.0 ACK but this is not the last packet", debugMsgLog);
                    if (ret) {
                        //delete ret;
                    }
                    return;
                }
                if(!sendACK(keepGoing)) {
                    if (lastResponse->length() > 0) {
                        // Didn't expect to get more data because we already received the last packet
                        // There is an additional KWP message following
                        keepGoing = true;
                        firstPacket = true;
                        emit log("Warning: Got a more than one message", debugMsgLog);
                    }
                    else {
                        emit log("Error: Error sending ACK", debugMsgLog);
                        if (ret) {
                            //delete ret;
                        }
                        return;
                    }
                }
            }
        }
    }
}

void tp20::writeToElmStr(const QString &str)
{
    QMetaObject::invokeMethod(elm, "write", Qt::QueuedConnection, Q_ARG(QString, str));
}

void tp20::writeToElmBA(const QByteArray &str)
{
    QMetaObject::invokeMethod(elm, "write", Qt::QueuedConnection, Q_ARG(QByteArray, str));
}

void tp20::setSendCanID(int id)
{
    QMetaObject::invokeMethod(elm, "setSendCanID", Qt::QueuedConnection, Q_ARG(int, id));
}

void tp20::setRecvCanID(int id)
{
    QMetaObject::invokeMethod(elm, "setRecvCanID", Qt::QueuedConnection, Q_ARG(int, id));
}

void tp20::setChannelClosed()
{
    channelDest = -1;
    emit channelOpened(false);
    return;
}

void tp20::elmInitialisationFailed()
{
    elmInitilised = false;
    emit elmInitDone(false);
    if (elm->getPortOpen())
        QMetaObject::invokeMethod(elm, "closePort", Qt::QueuedConnection);
    return;
}

void tp20::setSlowRecvTimeout(int slow)
{
    slowRecvTimeout = slow;
}

void tp20::setKeepAliveInterval(int time)
{
    keepAliveTimer.setInterval(time);
}

bool tp20::applyRecvTimeout(int msecs)
{
    if (msecs > 1020) {
        msecs = 1020;
    }
    if (msecs < 0) {
        msecs = 0;
    }

    // each increment is 4ms
    unsigned int val = msecs / 4;

    writeToElmStr("AT ST " + toHex(val));
    if (!getResponseStatus(OK_RESPONSE)) {
        emit log("Warning: Could not set receive timeout");
        recvTimeout = -1; // forces it to try again next time
        return false;
    }

    recvTimeout = msecs;
    return true;
}

bool tp20::sendACK(bool dataFollowing) {
    QByteArray ack;
    ack.append(0xB0 | (rxSeq & 0x0F));
    writeToElmBA(ack);
    if (!getResponseCAN(dataFollowing)) { // ECU should not respond to ACK, unless more TP data
        return false;
    }
    return true;
}

bool tp20::checkACK() {
    dataTrans dt = getAsDT(0);
    if (dt.opcode != 0xB || dt.seq != (txSeq & 0x0F)) {
        return false;
    }
    return true;
}

void tp20::initialiseElm(bool open)
{
    if (open) {
        // turn off echo
        writeToElmStr("AT E0"); // make sure there is no existing data coming from COM port
        getResponseStr();
        writeToElmStr("AT E0");
        if (!getResponseStatus(OK_RESPONSE)) {
            elmInitialisationFailed();
            return;
        }

        // get ID strings
        writeToElmStr("AT I");
        QString elmProtoVersion = getResponseStr();
        writeToElmStr("AT @1");
        QString devStr = getResponseStr();

        emit log("ID: " + devStr);
        emit log("Protocol: " + elmProtoVersion);

        writeToElmStr("ST I");
        QString stFirmware = getResponseStr();
        if (stFirmware != "?") {
            writeToElmStr("ST DI");
            QString stDevStr = getResponseStr();
            writeToElmStr("ST MFR");
            QString stMfr = getResponseStr();
            writeToElmStr("ST SN");
            QString stSN = getResponseStr();

            emit log("Manufacturer: " + stMfr);
            emit log("Device: " + stDevStr);
            emit log("Firmware: " + stFirmware);
            emit log("Serial: " + stSN);

            // set a pass all filter for ST devices
            writeToElmStr("ST FAP 000,000");
            if (!getResponseStatus(OK_RESPONSE)) {
                elmInitialisationFailed();
                return;
            }
        }

        // set user mode B to  500kbps, 11 bit ID
        writeToElmStr("AT PB C0 01");
        if (!getResponseStatus(OK_RESPONSE)) {
            elmInitialisationFailed();
            return;
        }

        // activate user mode B
        writeToElmStr("AT SP B");
        if (!getResponseStatus(OK_RESPONSE)) {
            elmInitialisationFailed();
            return;
        }

        // turn on CAN ID display (headers??)
        writeToElmStr("AT H1");
        if (!getResponseStatus(OK_RESPONSE)) {
            elmInitialisationFailed();
            return;
        }

        // turn on DLC display
        writeToElmStr("AT D1");
        if (!getResponseStatus(OK_RESPONSE)) {
            elmInitialisationFailed();
            return;
        }

        // make sure line feeds are off
        writeToElmStr("AT L0");
        if (!getResponseStatus(OK_RESPONSE)) {
            elmInitialisationFailed();
            return;
        }

        elmInitilised = true;
        emit elmInitDone(true);
    }
    else {
        elmInitilised = false;
        if (channelDest >= 0) {
            setChannelClosed();
        }
        return;
    }
}

void tp20::portClosed()
{
    elmInitilised = false;
    setChannelClosed();
    return;
}

bool tp20::checkResponse(int len)
{
    if (!lastResponse)
        return false;

    if (lastResponse->length() > 1 || lastResponse->at(0)->data.length() != len) {
        return false;
    }

    return true;
}

chanSetup tp20::getAsCS(int i)
{
    QByteArray* dataPtr = &(lastResponse->at(i)->data);

    chanSetup tmp;
    tmp.dest = static_cast<quint8>(dataPtr->at(0));
    tmp.opcode = static_cast<quint8>(dataPtr->at(1));
    tmp.rxID = static_cast<quint8>(dataPtr->at(2));
    tmp.rxV = (static_cast<quint8>(dataPtr->at(3) >> 4)) & 0x0F;
    tmp.rxPre = static_cast<quint8>(dataPtr->at(3)) & 0x0F;
    tmp.txID = static_cast<quint8>(dataPtr->at(4));
    tmp.txV = (static_cast<quint8>(dataPtr->at(5)) >> 4) & 0x0F;
    tmp.txPre = static_cast<quint8>(dataPtr->at(5)) & 0x0F;
    tmp.app = static_cast<quint8>(dataPtr->at(6));
    return tmp;
}

chanParam tp20::getAsCP(int i)
{
    QByteArray* dataPtr = &(lastResponse->at(i)->data);

    chanParam tmp;
    tmp.opcode = static_cast<quint8>(dataPtr->at(0));
    tmp.bs = static_cast<quint8>(dataPtr->at(1));
    tmp.T1 = static_cast<quint8>(dataPtr->at(2));
    tmp.T2 = static_cast<quint8>(dataPtr->at(3));
    tmp.T3 = static_cast<quint8>(dataPtr->at(4));
    tmp.T4 = static_cast<quint8>(dataPtr->at(5));
    return tmp;
}

dataTrans tp20::getAsDT(int i)
{
    QByteArray* dataPtr = &(lastResponse->at(i)->data);

    dataTrans tmp;
    tmp.opcode = (static_cast<quint8>(dataPtr->at(0)) >> 4) & 0x0F;
    tmp.seq = static_cast<quint8>(dataPtr->at(0)) & 0x0F;
    return tmp;
}

dataTransFirst tp20::getAsDTFirst(int i)
{
    QByteArray* dataPtr = &(lastResponse->at(i)->data);

    dataTransFirst tmp;
    tmp.opcode = (static_cast<quint8>(dataPtr->at(0)) >> 4) & 0x0F;
    tmp.seq = static_cast<quint8>(dataPtr->at(0)) & 0x0F;
    tmp.len = static_cast<quint8>(dataPtr->at(1)) << 8 | static_cast<quint8>(dataPtr->at(2));
    return tmp;
}

bool tp20::checkSeq()
{
    if (!lastResponse) {
        return false;
    }

    for (int i = 0; i < lastResponse->length(); i++) {
        dataTrans tmp = getAsDT(i);
        if (tmp.opcode > 0x3)
            continue;
        if (tmp.seq != (rxSeq++ & 0x0F)) {
            return false;
        }
    }
    return true;
}

void tp20::openChannel(int dest, int timeout)
{
    if (!elmInitilised) {
        return;
    }

    if (recvTimeout != timeout) {
        emit log("Info: Setting new timeout", debugMsgLog);
        if (!applyRecvTimeout(timeout)) {
            emit log("Error: Couldn't set timeout", debugMsgLog);
            setChannelClosed();
            return;
        }
    }

    setSendCanID(0x200);
    if (!getResponseStatus(OK_RESPONSE)) {
        setChannelClosed();
        return;
    }

    setRecvCanID(0x200 + dest);
    if (!getResponseStatus(OK_RESPONSE)) {
        setChannelClosed();
        return;
    }

    rxSeq = 0;
    txSeq = 0;

    writeToElmStr(toHex(dest, 2) + " C0 00 10 00 03 01");
    if (!getResponseCAN() || !checkResponse(7)) {
        setChannelClosed();
        return;
    }
    chanSetup setup = getAsCS(0);
    rxID = (setup.rxPre << 8) + setup.rxID;
    if (setup.opcode != 0xD0 || rxID != 0x300 || setup.rxV > 0 || setup.txV > 0) {
        setChannelClosed();
        return;
    }
    txID = (setup.txPre << 8) + setup.txID;

    setSendCanID(txID);
    if (!getResponseStatus(OK_RESPONSE)) {
        setChannelClosed();
        return;
    }

    setRecvCanID(rxID);
    if (!getResponseStatus(OK_RESPONSE)) {
        setChannelClosed();
        return;
    }

    writeToElmStr("A0 0F 8A FF 4A FF");
    if (!getResponseCAN() || !checkResponse(6)) {
        setChannelClosed();
        return;
    }
    chanParam param = getAsCP(0);
    if (param.opcode != 0xA1 || param.bs > 0xF) {
        setChannelClosed();
        return;
    }
    bs = param.bs;
    t1 = param.T1;
    t3 = param.T3;

    channelDest = dest;
    emit channelOpened(true);
}

void tp20::closeChannel()
{
    writeToElmStr("A8");
    getResponseCAN();
    setChannelClosed();
}

void tp20::sendKeepAlive()
{
    if (channelDest < 0)
        return;

    emit log("Sending keep alive command", keepAliveLog);

    //QMutexLocker locker(&sendLock);

    writeToElmStr("A3");
    if (!getResponseCAN() || !checkResponse(6)) {
        setChannelClosed();
        return;
    }
    chanParam param = getAsCP(0);
    if (param.opcode != 0xA1 || param.bs > 0xF) {
        setChannelClosed();
        return;
    }

    bs = param.bs;
    t1 = param.T1;
    t3 = param.T3;
}

bool tp20::getResponseStatus(int expectedResult)
{
    int status;

    bool tmp = elm->getResponseStatus(status);

    if (status == expectedResult) {
        return tmp;
    }

    emit log("Error: Wrong response while getting status", responseErrorLog);
    emit log(decodeError(status), responseErrorLog);

    return tmp;
}

bool tp20::checkForCommands() {
    for (int i = 0; i < lastResponse->length(); i++) {
        quint8 op = lastResponse->at(i)->data.at(0);
        if (op == 0xA3) { // channel test
            emit log("Received channel test command, sending response", keepAliveLog);
            writeToElmStr("A3");
            getResponseStr();

            // reset keep alive timer
            QMetaObject::invokeMethod(&keepAliveTimer, "start", Qt::QueuedConnection);

            lastResponse->removeAt(i--);
        }
        else if (op == 0xA8 || op == 0xA4) { // close channel, break
            setChannelClosed();
            return false;
        }
    }
    return true;
}

bool tp20::getResponseCAN(bool replyExpected)
{
    int status;

    if (lastResponse) {
        qDeleteAll(lastResponse->begin(), lastResponse->end());
        delete lastResponse;
        lastResponse = 0;
    }

    lastResponse = elm->getResponseCAN(status);
    if (!checkForCommands()) { // disconnect command must have occurred
        return false;
    }
    if (lastResponse->empty()) {
        status |= NO_DATA_RESPONSE; // account for removal of A3 tests
    }

    if (replyExpected && status == 0) {
        return true;
    }
    else if (!replyExpected && status == NO_DATA_RESPONSE) {
        return true;
    }
    else if (!replyExpected && status == 0) {
        //got data for ACK, probably should resend ACK
    }

    emit log("Error: Wrong response while getting CAN frame", responseErrorLog);
    emit log(decodeError(status), responseErrorLog);

    //setChannelClosed();
    return false;
}

QString tp20::getResponseStr()
{
    int status;

    QString tmp = elm->getResponseStr(status);

    if (status == 0) {
        return tmp;
    }

    emit log("Error: Wrong response while getting string", responseErrorLog);
    emit log(decodeError(status), responseErrorLog);
    return tmp;
}

QString tp20::decodeError(int status) {
    QStringList ret;

    if (status == 0) {
        ret << "Info: No status bits set";
    }

    if (status & TIMEOUT_ERROR) {
        ret << "Info: Timeout receiving data from ELM327";
    }

    if (status & NO_PROMPT_ERROR) {
        ret << "Info: No prompt from ELM327";
    }

    if (status & OK_RESPONSE) {
        ret << "Info: OK response from ELM327";
    }

    if (status & STOPPED_RESPONSE) {
        ret << "Info: STOPPED response from ELM327";
    }

    if (status & UNKNOWN_RESPONSE) {
        ret << "Info: UNKNOWN response from ELM327";
    }

    if (status & AT_RESPONSE) {
        ret << "Info: AT command in response from ELM327";
    }

    if (status & NO_DATA_RESPONSE) {
        ret << "Info: NO DATA response from ELM327";
    }

    if (status & PROCESSING_ERROR) {
        ret << "Info: Error parsing received data";
    }

    if (status & CAN_ERROR) {
        ret << "Info: CAN ERROR response from ELM327";
    }

    return ret.join("\n");
}
