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

#include "kwp2000.h"
#include "util.h"

#include <QDebug>
#include <QDateTime>
#include <QDir>
#include <QRegExp>
#include "qmath.h"

kwp2000::kwp2000(QObject *parent) :
    QObject(parent),
    nextBlock(0),
    readingBlocks(false),
    logFile(0),
    labelFile(0),
    doModuleRefresh(true),
    destModule(-1),
    slowRecvTimeout(40),
    normRecvTimeout(24),
    fastRecvTimeout(16)
{
    elmThread = new QThread(this);
    tpThread = new QThread(this);

    elm = new elm327();
    tp = new tp20(elm);

    elm->moveToThread(elmThread);
    tp->moveToThread(tpThread);

    elmThread->start();
    tpThread->start();

    connect(tp, SIGNAL(response(QByteArray*)), this, SLOT(recvKWP(QByteArray*)));

    connect(elm, SIGNAL(log(QString, int)), this, SIGNAL(log(QString, int)));
    connect(tp, SIGNAL(log(QString, int)), this, SIGNAL(log(QString, int)));

    connect(tp, SIGNAL(channelOpened(bool)), this, SLOT(channelOpenSlot(bool)));
    connect(tp, SIGNAL(channelOpened(bool)), this, SIGNAL(channelOpen(bool)));
    connect(tp, SIGNAL(elmInitDone(bool)), this, SIGNAL(elmInitialised(bool)));
    connect(tp, SIGNAL(elmInitDone(bool)), this, SLOT(openGW_refresh(bool)));

    connect(elm, SIGNAL(portOpened(bool)), this, SIGNAL(portOpened(bool)));
    connect(elm, SIGNAL(portClosed()), this, SIGNAL(portClosed()));

    readBlockTimer.setInterval(500);
    connect(&readBlockTimer, SIGNAL(timeout()), this, SLOT(readBlockTimeout()));

    initModuleNames();
}

kwp2000::~kwp2000()
{
    QMetaObject::invokeMethod(tp, "closeChannel", Qt::BlockingQueuedConnection);
    closePortBlocking();

    tpThread->exit(0);
    elmThread->exit(0);

    tpThread->wait(10000);
    elmThread->wait(10000);

    if (elmThread->isRunning()) {
        elmThread->terminate();
    }
    if (tpThread->isRunning()) {
        tpThread->terminate();
    }
}

void kwp2000::openPort()
{
    if (tp->getChannelDest() >= 0) {
        QMetaObject::invokeMethod(tp, "closeChannel", Qt::BlockingQueuedConnection);
    }
    QMetaObject::invokeMethod(elm, "openPort", Qt::QueuedConnection);
}

void kwp2000::closePort() {
    if (tp->getChannelDest() >= 0) {
        QMetaObject::invokeMethod(tp, "closeChannel", Qt::BlockingQueuedConnection);
    }
    QMetaObject::invokeMethod(elm, "closePort", Qt::QueuedConnection);
}

void kwp2000::closePortBlocking()
{
    if (tp->getChannelDest() >= 0) {
        QMetaObject::invokeMethod(tp, "closeChannel", Qt::BlockingQueuedConnection);
    }
    QMetaObject::invokeMethod(elm, "closePort", Qt::BlockingQueuedConnection);
}

void kwp2000::openChannel(int i) {
    if (getChannelDest() >= 0) {
        return;
    }

    if (i == 0) {
        return;
    }

    blockLabels.clear();

    int addr;
    if (moduleList.contains(i)) {
        addr = moduleList.value(i).addr;
    }
    else {
        addr = i;
    }

    destModule = i;

    emit log("Opening channel to module 0x" + toHex(destModule));
    QMetaObject::invokeMethod(tp, "openChannel", Qt::QueuedConnection,
                              Q_ARG(int, addr),
                              Q_ARG(int, normRecvTimeout));
}

void kwp2000::closeChannel() {
    emit log("Closing channel to module 0x" + toHex(destModule));
    QMetaObject::invokeMethod(tp, "closeChannel", Qt::QueuedConnection);
}

void kwp2000::setSerialParams(const serialSettings &in)
{
    if (tp->getChannelDest() >= 0) {
        QMetaObject::invokeMethod(tp, "closeChannel", Qt::BlockingQueuedConnection);
    }
    elm->setSerialParams(in);
}

int kwp2000::getChannelDest() const
{
    return tp->getChannelDest();
}

int kwp2000::getNumBlocksOpen() const
{
    return currentBlocks.keys().length();
}

bool kwp2000::getBlockOpen(int i) const
{
    return currentBlocks.contains(static_cast<quint8>(i));
}

bool kwp2000::getPortOpen() const
{
    return elm->getPortOpen();
}

bool kwp2000::getElmInitialised() const
{
    return tp->getElmInitialised();
}

void kwp2000::startDiag(int param)
{
    QByteArray packet;
    packet.append(0x10);
    packet.append(param);
    QMetaObject::invokeMethod(tp, "sendData", Qt::QueuedConnection,
                              Q_ARG(QByteArray, packet),
                              Q_ARG(int, slowRecvTimeout));
}

void kwp2000::openBlock(int blockNum)
{
    if (!currentBlocks.contains(blockNum)) {
        currentBlocks.insert(blockNum, QVector<blockValue>(4));

        // set descriptions from label file

        emit blockOpen(blockNum);
    }

    changeSampleFormat();

    if (!readingBlocks) {
        readingBlocks = true;
        readBlocks();
    }
}

void kwp2000::closeBlock(int blockNum)
{
    currentBlocks.remove(static_cast<quint8>(blockNum));
    emit blockClosed(blockNum);
    changeSampleFormat();
}

void kwp2000::closeAllBlocks()
{
    QList<int> openBlocks = currentBlocks.keys();
    currentBlocks.clear();

    for (int i = 0; i < openBlocks.length(); i++) {
        emit blockClosed(openBlocks.at(i));
    }

    changeSampleFormat();
}

void kwp2000::channelOpenSlot(bool status)
{
    if (status == false) {
        if (destModule >= 0) {
            emit log("Channel closed to module 0x" + toHex(destModule));
        }
        destModule = -1;

        modulePartNum.clear();
        closeAllBlocks();
        emit labelsLoaded(false);
    }
    else {
        emit log("Channel opened to module 0x" + toHex(destModule));

        if (tp->getChannelDest() == 31 && doModuleRefresh) {
            QByteArray tmp;
            tmp.append(0x1A);
            tmp.append(0x9F);
            QMetaObject::invokeMethod(tp, "sendData", Qt::QueuedConnection,
                                      Q_ARG(QByteArray, tmp),
                                      Q_ARG(int, slowRecvTimeout));
        }
        else {
            // when channel is opened, start diagnostic session
            // when diag session started the ID strings will be retrieved
            startDiag();
        }
    }
}

void kwp2000::miscCommand(const QByteArray &cmd)
{
    QMetaObject::invokeMethod(tp, "sendData", Qt::QueuedConnection,
                              Q_ARG(QByteArray, cmd),
                              Q_ARG(int, slowRecvTimeout));
}

void kwp2000::changeSampleFormat()
{
    sample.clear();

    QList<int> openBlocks = currentBlocks.keys();
    for (int i = 0; i < openBlocks.length(); i++) {
        int blockNum = openBlocks.at(i);

        for (int pos = 0; pos < 4; pos++) {
            blockRef tmpRef = {blockNum, pos};

            bool matchFound = false;

            // check and see if an equivilent sampleValue already exists
            if (pos == 0 && blockLabels[blockNum].desc[0].toLower() == "engine speed") {
                for (int sampleNum = 0; sampleNum < sample.length(); sampleNum++) {
                    int existingBlock = sample[sampleNum].refs[0].blockNum;
                    int existingPos = sample[sampleNum].refs[0].pos;

                    if (existingPos == 0 && blockLabels[existingBlock].desc[0].toLower() == "engine speed") {
                        matchFound = true;
                        currentBlocks[blockNum][pos].indexToSampleValue = sampleNum;
                        sample[sampleNum].refs.append(tmpRef);
                    }
                }
            }

            if (!matchFound) {
                // insert new sampleValue
                sampleValue value;
                value.refs.append(tmpRef);
                currentBlocks[blockNum][pos].indexToSampleValue = sample.length();
                sample.append(value);
            }
        }
    }

    emit sampleFormatChanged();
}

void kwp2000::startLogging()
{
    QDir(".").mkdir("Logs");

    logFileName = "Logs/" + QDateTime::currentDateTime().toString("yyyy-MM-dd_HH-mm-ss") + ".csv";

    logFile = new QFile(logFileName, this);
    logFile->open(QIODevice::WriteOnly | QIODevice::Text);
    logOut.setDevice(logFile);

    // print header
    logOut << "Time";
    for (int i = 0; i < sample.length(); i++) {
        int sampleBlockNum = sample[i].refs[0].blockNum;
        int samplePos = sample[i].refs[0].pos;
        logOut << "," + blockLabels[sampleBlockNum].desc[samplePos];
        logOut << " " + blockLabels[sampleBlockNum].subDesc[samplePos];
        logOut << " [" + currentBlocks[sampleBlockNum][samplePos].units + "]";
    }
    logOut << endl;
    logOut.flush();

    emit loggingStarted();
}

void kwp2000::stopLogging()
{
    logOut.setDevice(0);
    logFile->close();
    delete logFile;
    logFile = 0;
}

void kwp2000::loadLabelFile()
{
    if (modulePartNum.empty()) {
        emit labelsLoaded(false);
        return;
    }

    QString partNum;
    int end = modulePartNum.at(0).indexOf(QChar(' '));
    if (end < 0) {
        partNum = modulePartNum.at(0);
    }
    else {
        partNum = modulePartNum.at(0).left(end);
    }

    QStringList partSplit;
    for (int i = 0; i < partNum.length(); i += 3) {
        partSplit << partNum.mid(i, 3);
    }
    partNum = partSplit.join("-");

    if (labelDir.length() == 0) {
        emit labelsLoaded(false);
        return;
    }

    QDir dir(labelDir);
    if (!dir.exists()) {
        emit labelsLoaded(false);
        return;
    }

    labelFileName = dir.absolutePath() + "/User/" + partNum + ".lbl";
    labelFile = new QFile(labelFileName, this);

    // try to find labels from direct part name
    // check user folder first, then root label dir
    if (!labelFile->exists()) {
        labelFileName = dir.absolutePath() + "/" + partNum + ".lbl";
        labelFile->setFileName(labelFileName);

        if (!labelFile->exists()) {
            // try to find labels from redirect files
            // check user folder first, then root label dir
            QString fileName = findLabelFromRedir(partNum, dir.absolutePath() + "/User");
            if (fileName.length() == 0) {
                fileName = findLabelFromRedir(partNum, dir.absolutePath());
            }
            if (fileName.length() == 0) {
                emit labelsLoaded(false);
                return;
            }

            // got label file name from redirect, now find file in user or root dir
            labelFileName = dir.absolutePath() + "/User/" + fileName;
            labelFile->setFileName(labelFileName);

            if (!labelFile->exists()) {
                labelFileName = dir.absolutePath() + "/" + fileName;
                labelFile->setFileName(labelFileName);

                if (!labelFile->exists()) {
                    emit labelsLoaded(false);
                    return;
                }
            }
        }
    }

    labelFile->open(QIODevice::ReadOnly);
    labelIn.setDevice(labelFile);

    int blockNum = -1;
    int pos = -1;
    while(!labelIn.atEnd()) {
        QString line = labelIn.readLine();
        if (line.length() > 0) {
            char firstChar = line.at(0).toAscii();
            if (firstChar >= 48 && firstChar <= 57) {
                bool ok;

                QStringList parts = line.split(QChar(','));
                if (parts.length() < 3) {
                    continue;
                }

                blockNum = parts.at(0).toInt(&ok);
                if (!ok || blockNum < 0 || blockNum > 255) {
                    continue;
                }

                pos = parts.at(1).toInt(&ok);
                if (!ok || pos < 0 || pos > 4) {
                    continue;
                }

                if (!blockLabels.contains(blockNum)) {
                    blockLabels_t newBlockLabel;
                    blockLabels.insert(blockNum, newBlockLabel);
                }

                if (pos == 0) {
                    blockLabels[blockNum].blockName = parts.at(2);
                    continue;
                }

                blockLabels[blockNum].desc[pos-1] = parts.at(2);
                if (parts.length() > 3) {
                    blockLabels[blockNum].subDesc[pos-1] = parts.at(3);
                }
                if (parts.length() > 4) {
                    QString longDesc = parts.at(4);
                    blockLabels[blockNum].longDesc[pos-1] = longDesc.replace("\\n", "\n");
                }
            }

            // binary description
            if (firstChar == ';' && line.contains(QChar('='))) {
                if (blockNum < 0 || blockNum > 255 || pos < 0 || pos > 4) {
                    continue;
                }
                if (!blockLabels.contains(blockNum)) {
                    continue;
                }
                blockLabels[blockNum].binDesc[pos-1] += line.mid(2) + '\n';
            }
        }
    }

    if (blockLabels.count() > 0) {
        emit labelsLoaded(true);
    }
    else {
        emit labelsLoaded(false);
    }

    labelIn.setDevice(0);
    labelFile->close();
    delete labelFile;
    labelFile = 0;
}

void kwp2000::openGW_refresh(bool ok)
{
    if (ok) {
        doModuleRefresh = true;
        QMetaObject::invokeMethod(tp, "openChannel", Qt::QueuedConnection,
                                  Q_ARG(int, 31),
                                  Q_ARG(int, normRecvTimeout));
    }
}

void kwp2000::readBlocks()
{
    QList<int> openBlocks = currentBlocks.keys();
    if (openBlocks.empty()) {
        nextBlock = 0;
        readingBlocks = false;
        return;
    }

    if (nextBlock >= openBlocks.length()) {
        nextBlock = 0;
    }

    QByteArray packet;
    packet.append(0x21);
    packet.append(openBlocks.at(nextBlock++));

    QMetaObject::invokeMethod(tp, "sendData", Qt::QueuedConnection,
                              Q_ARG(QByteArray, packet),
                              Q_ARG(int, fastRecvTimeout));
    readBlockTimer.start();
}

void kwp2000::recvKWP(QByteArray *data)
{
    if (!data) {
        emit log("Error: Received empty KWP data");
        QMetaObject::invokeMethod(tp, "closeChannel", Qt::BlockingQueuedConnection);
        return;
    }

    if (data->length() < 2) {
        emit log("Error: Received malformed KWP data");
        QMetaObject::invokeMethod(tp, "closeChannel", Qt::BlockingQueuedConnection);
        delete data;
        return;
    }

    quint8 respCode = static_cast<quint8>(data->at(0));
    quint8 param = static_cast<quint8>(data->at(1));
    data->remove(0,2);

    if (respCode == 0x7F) {
        emit log("Warning: Received negative KWP response to " + toHex(param) + " command");
        if (data->length() > 0) {
            quint8 reasonCode = static_cast<quint8>(data->at(0));
            emit log("Reason code " + toHex(reasonCode));
        }
        delete data;
        return;
    }

    switch (respCode) {
    case 0x50:
        startDiagHandler(data, param);
        break;
    case 0x5A:
        if (param == 0x91) {
            shortIdHandler(data);
        }
        else if (param == 0x9B) {
            longIdHandler(data);
        }
        else if (param == 0x9F) {
            queryModulesHandler(data);
        }
        else {
            miscHandler(data, respCode, param);
        }
        break;
    case 0x61:
        blockDataHandler(data, param);
        break;
    default:
        miscHandler(data, respCode, param);
        break;
    }
}

void kwp2000::readBlockTimeout()
{
    readBlocks();
}

void kwp2000::blockDataHandler(QByteArray *data, quint8 param)
{
    quint8 blockNum = param;

    if (!currentBlocks.contains(blockNum)) {
        emit log("Warning: Got values for block " + QString::number(blockNum) + " but its not open.");
        delete data;
        readBlocks();
        return;
    }

    for (int i = 0; i < 4; i++) {
        QString units;
        QVariant val = decodeBlockData(data->at(i*3), data->at(i*3+1), data->at(i*3+2), units);

        currentBlocks[blockNum][i].units = units;
        currentBlocks[blockNum][i].val = val;
    }

    emit newBlockData(blockNum);

    updateSample(blockNum);

    if (logFile) {
        writeSample();
    }

    delete data;
    readBlocks();
}

void kwp2000::startDiagHandler(QByteArray *data, quint8 param)
{
    emit diagStarted(param);
    delete data;

    // do request for long ID
    QByteArray tmp;
    tmp.append(0x1A);
    tmp.append(0x9B);
    QMetaObject::invokeMethod(tp, "sendData", Qt::QueuedConnection,
                              Q_ARG(QByteArray, tmp),
                              Q_ARG(int, slowRecvTimeout));
}

void kwp2000::miscHandler(QByteArray *data, quint8 respCode, quint8 param)
{
    emit log("Misc command: Response code" + toHex(respCode) + ", parameter " + toHex(param));
    delete data;
}

void kwp2000::shortIdHandler(QByteArray *data) {
    QStringList tmpList;

    // rewrite to use interpretRawData()
    for (int i = 0; i < data->length();) {
        if (static_cast<quint8>(data->at(i)) == 0xFF) {
            break;
        }
        QString tmp;
        quint8 len = static_cast<quint8>(data->at(i));
        for (int j = 1; j < len; j++) {
            tmp += QChar(data->at(i+j));
        }
        i += len;

        while(tmp.right(1) == QChar(' ')) {
            tmp.chop(1);
        }
        tmpList << tmp;
    }

    ecuPartNum = tmpList;
    emit newEcuInfo(ecuPartNum);

    delete data;
}

void kwp2000::longIdHandler(QByteArray *data)
{
    QStringList tmpList;
    QString tmp;

    for (int i = 0; i < 16; i++) {
        tmp += QChar(data->at(i));
    }
    tmpList << tmp;

    tmp.clear();
    for (int i = 26; i < data->length(); i++) {
        tmp += QChar(data->at(i));
    }
    while(tmp.right(1) == QChar(' ')) {
        tmp.chop(1);
    }
    tmpList << tmp;

    modulePartNum = tmpList;
    emit newModuleInfo(modulePartNum);
    loadLabelFile();

    // now send request for short ID
    QByteArray tmpBA;
    tmpBA.append(0x1A);
    tmpBA.append(0x91);
    QMetaObject::invokeMethod(tp, "sendData", Qt::QueuedConnection,
                              Q_ARG(QByteArray, tmpBA),
                              Q_ARG(int, slowRecvTimeout));

    delete data;
}

void kwp2000::queryModulesHandler(QByteArray *data)
{
    QList<QByteArray> dataList = interpretRawData(data);
    delete data;

    if (dataList.length() != 2) {
        emit log("List modules: Didn't get list of 2 byte arrays");
        QMetaObject::invokeMethod(tp, "closeChannel", Qt::BlockingQueuedConnection);
        return;
    }

    moduleList.clear();

    for (int i = 0; i < dataList.at(0).length(); i+=4) {
        if (i+4 > dataList.at(0).length()) {
            emit log("List modules: Not divisable by 4.");
            QMetaObject::invokeMethod(tp, "closeChannel", Qt::BlockingQueuedConnection);
            return;
        }

        quint8 lsb = static_cast<quint8>(dataList.at(0).at(i+3));
        if (lsb) {
            moduleInfo_t tmp;
            tmp.number = static_cast<quint8>(dataList.at(0).at(i));
            tmp.addr = static_cast<quint8>(dataList.at(0).at(i+1));
            tmp.name = moduleNames.value(tmp.number, "Unknown Module");
            tmp.isPresent = ((lsb & 0x01) == 1);
            tmp.status = (lsb & 0x1E) >> 1;

            // skip modules with address of 0x13, module numbers 0x0 and 0x4 seem to report this addr
            // but these modules dont exist
            if (tmp.addr == 0x13) {
                emit log("Skipping module " + toHex(tmp.number) + " (addr is 0x13)", debugMsgLog);
                continue;
            }

            moduleList.insert(tmp.number, tmp);
        }
    }

    emit moduleListRefreshed();
    doModuleRefresh = false;
    QMetaObject::invokeMethod(tp, "closeChannel", Qt::BlockingQueuedConnection);
}

void kwp2000::writeSample()
{
    logOut << QTime::currentTime().toString("HH:mm:ss.zzz");
    for (int i = 0; i < sample.length(); i++) {
        logOut << "," + sample.at(i).val.toString();
    }
    logOut << endl;
    logOut.flush();
}

void kwp2000::updateSample(int block)
{
    for (int i = 0; i < 4; i++) {
        int sampleIndex = currentBlocks[block][i].indexToSampleValue;
        sample[sampleIndex].val = currentBlocks[block][i].val;
    }
}

QString kwp2000::findLabelFromRedir(QString partNum, QString dirStr)
{
    int moduleNum = tp->getChannelDest();

    if (partNum.length() < 11 || partNum.length() > 15 || moduleNum < 0 || moduleNum > 255) {
        return QString();
    }

    QDir dir(dirStr);
    if (!dir.exists()) {
        return QString();
    }

    QString redirFile = partNum.left(2) + QString("-%1.lbl").arg(moduleNum, 2, 16, QChar('0'));
    QString redirFileFull = dir.absolutePath() + "/" + redirFile;

    QFile file(redirFileFull);
    if (!file.exists()) {
        return QString();
    }

    file.open(QIODevice::ReadOnly);
    QTextStream in(&file);
    while (!in.atEnd()) {
        QString line = in.readLine();
        if (line.length() > 0 && line.startsWith("REDIRECT")) {
            line.remove(QChar(' '));
            int comment = line.indexOf(QChar(';'));
            if (comment >= 0) {
                line = line.left(comment);
            }
            QStringList split = line.split(QChar(','));

            QString partFromLabel = split.at(2);
            while (partFromLabel.right(1) == QChar('?') &&
                   partFromLabel.length() > partNum.length()) {
                partFromLabel.chop(1);
            }
            QRegExp match(partFromLabel, Qt::CaseInsensitive, QRegExp::Wildcard);

            if (match.exactMatch(partNum)) {
                QString tmp = split.at(1);
                tmp.replace(".LBL", ".lbl");
                tmp.replace(".CLB", ".lbl");
                return tmp;
            }
        }
    }

    return QString();
}

// should rewrite to not work char by char
QList<QByteArray> kwp2000::interpretRawData(QByteArray *raw)
{
    QList<QByteArray> retList;

    for (int i = 0; i < raw->length();) {
        if (static_cast<quint8>(raw->at(i)) == 0xFF) {
            break;
        }
        QByteArray tmp;
        quint8 len = static_cast<quint8>(raw->at(i));
        for (int j = 1; j < len; j++) {
            tmp += static_cast<quint8>(raw->at(i+j));
        }
        i += len;

        retList << tmp;
    }

    return retList;
}

void kwp2000::initModuleNames()
{
    moduleNames.insert(0x01, "Engine #1");
    moduleNames.insert(0x02, "Transmission");
    moduleNames.insert(0x03, "ABS");
    moduleNames.insert(0x05, "Security Access");
    moduleNames.insert(0x06, "Passenger Seat");
    moduleNames.insert(0x07, "Front Info/Control");
    moduleNames.insert(0x08, "AC & Heating");
    moduleNames.insert(0x09, "Central Electronics #1");
    moduleNames.insert(0x10, "Parking Aid #2");
    moduleNames.insert(0x11, "Engine #2");
    moduleNames.insert(0x13, "Distance Regulation");
    moduleNames.insert(0x14, "Suspension");
    moduleNames.insert(0x15, "Airbags");
    moduleNames.insert(0x16, "Steering");
    moduleNames.insert(0x17, "Instrument Cluster");
    moduleNames.insert(0x18, "Aux Heater");
    moduleNames.insert(0x19, "CAN Gateway");
    moduleNames.insert(0x20, "High Beam Assist");
    moduleNames.insert(0x22, "All Wheel Drive");
    moduleNames.insert(0x25, "Immobiliser");
    moduleNames.insert(0x26, "Convertible Top");
    moduleNames.insert(0x29, "Left Headlight");
    moduleNames.insert(0x31, "Diagnostic Interface");
    moduleNames.insert(0x34, "Level Control");
    moduleNames.insert(0x35, "Central Locking");
    moduleNames.insert(0x36, "Driver Seat");
    moduleNames.insert(0x37, "Radio/Sat Nav");
    moduleNames.insert(0x39, "Right Headlight");
    moduleNames.insert(0x42, "Driver Door");
    moduleNames.insert(0x44, "Steering Assist");
    moduleNames.insert(0x45, "Interior Monitoring");
    moduleNames.insert(0x46, "Comfort System");
    moduleNames.insert(0x47, "Sound System");
    moduleNames.insert(0x52, "Passenger Door");
    moduleNames.insert(0x53, "Parking Brake");
    moduleNames.insert(0x55, "Headlights");
    moduleNames.insert(0x56, "Radio");
    moduleNames.insert(0x57, "TV Tuner");
    moduleNames.insert(0x61, "Battery");
    moduleNames.insert(0x62, "Rear Left Door");
    moduleNames.insert(0x65, "Tire Pressure");
    moduleNames.insert(0x67, "Voice Control");
    moduleNames.insert(0x68, "Wipers");
    moduleNames.insert(0x69, "Trailer Recognition");
    moduleNames.insert(0x72, "Rear Right Door");
    moduleNames.insert(0x75, "Telematics");
    moduleNames.insert(0x76, "Parking Aid");
    moduleNames.insert(0x77, "Telephone");
}

QVariant kwp2000::decodeBlockData(quint8 id, quint8 a, quint8 b, QString &units)
{
    //qDebug() << "ID " << id << QString::number(id, 16) << " A " << a << " B " << b << flush;

    unsigned int uint;
    double dbl;
    QString ret;

    switch (id) {
    case 0x01:
        units = "rpm";
        dbl = a*b/5.0;
        return dbl;
    case 0x04:
        if (b > 127) {
            units = QString::fromUtf8("\u00B0 ATDC");
        }
        else {
            units = QString::fromUtf8("\u00B0 BTDC");
        }
        dbl = abs(b-127)*0.01*a;
        return dbl;
    case 0x07:
        units = "km/h";
        dbl = 0.01*a*b;
        return dbl;
    case 0x08:
        units = "Binary";
        uint = (a << 8 | b);
        return uint;
    case 0x10:
        units = "Binary";
        uint = (a << 8 | b);
        return uint;
    case 0x11:
        units = "ASCII";
        ret = QChar::fromAscii(a);
        ret += QChar::fromAscii(b);
        return ret;
    case 0x12:
        units = "mbar";
        dbl = a*b/25.0;
        return dbl;
    case 0x14:
        units = "%";
        dbl = a*b/128.0-1;
        return dbl;
    case 0x15:
        units = "V";
        dbl = a*b/1000.0;
        return dbl;
    case 0x16:
        units = "ms";
        dbl = 0.001*a*b;
        return dbl;
    case 0x17:
        units = "%";
        dbl = b*a/256.0;
        return dbl;
    case 0x1A:
        units = QString::fromUtf8("\u00B0 C");
        dbl = b-a;
        return dbl;
    case 0x21:
        units = "%";
        if (a == 0)
            dbl = 100.0*b;
        else
            dbl = 100.0*b/a;
        return dbl;
    case 0x22:
        units = "kW";
        dbl = (b-128)*0.01*a;
        return dbl;
    case 0x23:
        units = "l/h";
        dbl = a*b/100.0;
        return dbl;
    case 0x25:
        units = "Binary";
        uint = (a << 8 | b);
        return uint;
    case 0x27:
        // fuel
        units = "mg/stk";
        dbl = a*b/256.0;
        return dbl;
    case 0x31:
        // air
        units = "mg/stk";
        dbl = a*b/40.0;
        return dbl;
    case 0x33:
        units = QString::fromUtf8("mg/stk \u0394");
        dbl = ((b-128)/255.0)*a;
        return dbl;
    case 0x36:
        units = "Count";
        dbl = a*256+b;
        return dbl;
    case 0x37:
        units = "s";
        dbl = a*b/200.0;
        return dbl;
    case 0x51:
        units = QString::fromUtf8("\u00B0 CF");
        dbl = ((a*112000.0) + (b*436.0))/1000.0; // Torsion, check formula
        return dbl;
    case 0x5E:
        units = "Nm";
        dbl = a*(b/50.0-1); // Torque, check formula
        return dbl;
    default:
        units = "Raw";
        uint = (a << 8 | b);
        return uint;
    }
}

QVariant kwp2000::getBlockValue(int blockNum, int pos)
{
    if (currentBlocks.contains(blockNum)) {
        return currentBlocks[blockNum][pos].val;
    }
    else return QVariant();
}

QString kwp2000::getBlockUnits(int blockNum, int pos)
{
    if (currentBlocks.contains(blockNum)) {
        return currentBlocks[blockNum][pos].units;
    }
    else return QString();
}

QString kwp2000::getBlockDesc(int blockNum, int pos)
{
    if (currentBlocks.contains(blockNum)) {
        return currentBlocks[blockNum][pos].desc;
    }
    else return QString();
}

blockLabels_t kwp2000::getBlockLabel(int blockNum)
{
    blockLabels_t empty;
    return blockLabels.value(blockNum, empty);
}

QString kwp2000::getLabelFileName()
{
    return labelFileName;
}

void kwp2000::setLabelDir(QString dir)
{
    labelDir = dir;
}

QString kwp2000::getLabelDir()
{
    return labelDir;
}

const QMap<int, moduleInfo_t> &kwp2000::getModuleList() const
{
    return moduleList;
}

const QList<sampleValue> &kwp2000::getSample() const
{
    return sample;

    /*
    static double x = 0;
    x += 2.0*M_PI / 20.0;

    static QList<sampleValue> tmp;

    if (tmp.empty()) {
        sampleValue tmpSample1;
        for (int i = 0; i < 16; i++) {
            tmp.append(tmpSample1);
        }
    }

    for (int i = 0; i < 16; i++) {
        tmp[i].val = qSin(x + (2.0*M_PI / 16.0)*i) * 10 * (i+1);
    }

    return tmp;
    */
}

QFileInfo kwp2000::getLogfileInfo()
{
    if (!logFile) {
        return QFileInfo();
    }
    return QFileInfo(*logFile);
}

void kwp2000::setTimeouts(int slow, int norm, int fast)
{
    slowRecvTimeout = slow;
    normRecvTimeout = norm;
    fastRecvTimeout = fast;
    tp->setSlowRecvTimeout(slow);
}

void kwp2000::setKeepAliveInterval(int time)
{
    tp->setKeepAliveInterval(time);
}
