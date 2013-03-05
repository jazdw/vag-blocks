#ifndef TP20_H
#define TP20_H

#include <QObject>
#include <QTimer>

#include "elm327.h"
#include "canframe.h"
#include "util.h"

typedef struct {
    quint8 dest;
    quint8 opcode;
    quint8 rxID;
    quint8 rxV;
    quint8 rxPre;
    quint8 txID;
    quint8 txV;
    quint8 txPre;
    quint8 app;
} chanSetup;

typedef struct {
    quint8 opcode;
    quint8 bs;
    quint8 T1;
    quint8 T2;
    quint8 T3;
    quint8 T4;
} chanParam;

typedef struct {
    quint8 opcode;
    quint8 bs;
} chanParamShort;

typedef struct {
    quint8 opcode;
    quint8 seq;
} dataTrans;

typedef struct {
    quint8 opcode;
    quint8 seq;
    quint16 len;
} dataTransFirst;

class tp20 : public QObject
{
    Q_OBJECT
public:
    tp20(elm327* elm, QObject *parent = 0);
    int getChannelDest();
    bool getElmInitialised();
    void setSlowRecvTimeout(int slow);
    void setKeepAliveInterval(int time);
public slots:
    void initialiseElm(bool open);
    void portClosed();
    void openChannel(int dest, int timeout);
    void closeChannel();
    void sendData(const QByteArray &data, int requestedTimeout);
private slots:
    void sendKeepAlive();
signals:
    void log(const QString &txt, int logLevel = stdLog);
    void elmInitDone(bool ok);
    void channelOpened(bool ok);
    void response(QByteArray* data);
private:
    elm327* elm;
    QList<canFrame*>* lastResponse;
    int channelDest;
    quint16 txID;
    quint16 rxID;
    quint8 bs;
    quint8 t1;
    quint8 t3;
    quint8 txSeq;
    quint8 rxSeq;
    QTimer keepAliveTimer;
    QMutex sendLock;
    bool elmInitilised;

    bool getResponseCAN(bool replyExpected = true);
    QString getResponseStr();
    bool getResponseStatus(int expectedResult);

    bool checkResponse(int len);
    chanSetup getAsCS(int i);
    chanParam getAsCP(int i);
    dataTrans getAsDT(int i);
    dataTransFirst getAsDTFirst(int i);

    bool checkSeq();
    bool checkACK();
    bool sendACK(bool dataFollowing = false);
    bool checkForCommands();

    void recvData();

    void writeToElmStr(const QString &str);
    void writeToElmBA(const QByteArray &str);
    void setSendCanID(int id);
    void setRecvCanID(int id);

    void setChannelClosed();
    void elmInitialisationFailed();
    QString decodeError(int status);

    bool applyRecvTimeout(int msecs);
    int recvTimeout;
    int slowRecvTimeout;
};

#endif // TP20_H
