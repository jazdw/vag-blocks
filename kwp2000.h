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

#ifndef KWP2000_H
#define KWP2000_H

#include <QObject>
#include <QThread>
#include <QVariant>
#include <QMap>
#include <QVector>
#include <QFile>
#include <QTextStream>
#include <QFileInfo>
#include "serialport.h"
#include "elm327.h"
#include "tp20.h"
#include "util.h"
#include "serialsettings.h"

typedef struct {
    int blockNum;
    int pos;
} blockRef;

typedef struct {
    QList<blockRef> refs;
    QVariant val;
} sampleValue;

typedef struct {
    QString units;
    QString desc;
    QVariant val;
    int indexToSampleValue;
} blockValue;

typedef struct {
    QString blockName;
    QString desc[4];
    QString subDesc[4];
    QString longDesc[4];
    QString binDesc[4];
} blockLabels_t;

typedef struct {
    int number;
    int addr;
    QString name;
    bool isPresent;
    int status;
} moduleInfo_t;

class kwp2000 : public QObject
{
    Q_OBJECT
public:
    explicit kwp2000(QObject *parent = 0);
    ~kwp2000();
    void setSerialParams(const serialSettings &in);
    int getChannelDest() const;
    int getNumBlocksOpen() const;
    bool getBlockOpen(int i) const;
    bool getPortOpen() const;
    bool getElmInitialised() const;
    static QVariant decodeBlockData(quint8 id, quint8 a, quint8 b, QString &units);
    QVariant getBlockValue(int blockNum, int pos);
    QString getBlockUnits(int blockNum, int pos);
    QString getBlockDesc(int blockNum, int pos);
    blockLabels_t getBlockLabel(int blockNum);
    QString getLabelFileName();
    void setLabelDir(QString dir);
    QString getLabelDir();
    const QMap<int, moduleInfo_t>& getModuleList() const;
    const QList<sampleValue>& getSample() const;
    QFileInfo getLogfileInfo();
    void setTimeouts(int slow, int norm, int fast);
    void setKeepAliveInterval(int time);
signals:
    void log(const QString &txt, int logLevel = stdLog);
    void diagStarted(int param);
    void newBlockData(int blockNum);
    void channelOpen(bool status);
    void blockOpen(int num);
    void blockClosed(int num);
    void portOpened(bool open);
    void portClosed();
    void elmInitialised(bool ok);
    void newEcuInfo(QStringList info);
    void newModuleInfo(QStringList info);
    void labelsLoaded(bool ok);
    void moduleListRefreshed();
    void sampleFormatChanged();
    void loggingStarted();
public slots:
    void openPort();
    void closePort();
    void closePortBlocking();
    void openChannel(int i);
    void closeChannel();
    void startDiag(int param = 0x89);
    void openBlock(int blockNum);
    void closeBlock(int blockNum);
    void closeAllBlocks();
    void channelOpenSlot(bool status);
    void miscCommand(const QByteArray &cmd);
    void startLogging();
    void stopLogging();
    void loadLabelFile();
    void openGW_refresh(bool ok = true);
private slots:
    void recvKWP(QByteArray* data);
    void readBlockTimeout();
private:
    QThread* elmThread;
    QThread* tpThread;
    elm327* elm;
    tp20* tp;

    void readBlocks();
    int nextBlock;
    bool readingBlocks;
    QTimer readBlockTimer;

    void blockDataHandler(QByteArray* data, quint8 param);
    void startDiagHandler(QByteArray* data, quint8 param);
    void miscHandler(QByteArray* data, quint8 respCode, quint8 param);
    void longIdHandler(QByteArray* data);
    void shortIdHandler(QByteArray* data);
    void queryModulesHandler(QByteArray* data);

    QString logFileName;
    QFile* logFile;
    QTextStream logOut;

    QString labelFileName;
    QFile* labelFile;
    QTextStream labelIn;

    QMap<int, QVector<blockValue> > currentBlocks;
    QList<sampleValue> sample;
    void writeSample();
    void updateSample(int block);

    QMap<int, blockLabels_t> blockLabels;

    QStringList modulePartNum;
    QStringList ecuPartNum;

    QString labelDir;
    QString findLabelFromRedir(QString partNum, QString dirStr);

    QList<QByteArray> interpretRawData(QByteArray* raw);
    QMap<int, moduleInfo_t> moduleList;
    QMap<int, int> addrToModuleMap;

    void initModuleNames();
    QMap<int, QString> moduleNames;

    bool doModuleRefresh;

    void changeSampleFormat();

    int destModule;

    int slowRecvTimeout;
    int normRecvTimeout;
    int fastRecvTimeout;
};

#endif // KWP2000_H
