#include "elm327.h"
#include "util.h"

elm327::elm327(QObject *parent) :
    QObject(parent),
    port(0),
    protectLines(new QMutex()),
    sendCanID(0), recvCanID(0),
    portOpen(false)
{
    // default settings
    settings.name = "COM1";
    settings.dataBits = SerialPort::Data8;
    settings.flowControl = SerialPort::NoFlowControl;
    settings.parity = SerialPort::NoParity;
    settings.rate = SerialPort::Rate115200;
    settings.stopBits = SerialPort::OneStop;
}

elm327::~elm327()
{
    closePort();
}

void elm327::openPort()
{
    if (!port) {
        port = new SerialPort(this);
        port->setReadBufferSize(0x100000);
        connect(port, SIGNAL(readyRead()), this, SLOT(constructLine()));
    }

    closePort();

    port->setPort(settings.name);

    if (port && port->open(QIODevice::ReadWrite)) {
        emit log("Port opened.");

        bool ok;
        ok = port->setRate(settings.rate);
        if (!ok) {
            emit log("Serial port rate could not be set.", serialConfigLog);
        }
        ok = port->setDataBits(settings.dataBits);
        if (!ok) {
            emit log("Serial port data bits could not be set.", serialConfigLog);
        }
        ok = port->setParity(settings.parity);
        if (!ok) {
            emit log("Serial port parity could not be set.", serialConfigLog);
        }
        ok = port->setStopBits(settings.stopBits);
        if (!ok) {
            emit log("Serial port stop bits could not be set.", serialConfigLog);
        }
        ok = port->setFlowControl(settings.flowControl);
        if (!ok) {
            emit log("Serial port flow control could not be set.", serialConfigLog);
        }

        emit log("Port configured.");
        portOpen = true;
        emit portOpened(true);
        return;
    }
    portOpen = false;
    emit portOpened(false);
    emit log("Port could not be opened.");
    return;
}

void elm327::closePort()
{
    if (port) {
        port->close();
        emit portClosed();
        emit log("Port closed.");
    }
    portOpen = false;
}

void elm327::write(const QString &txt)
{
    //if (txt2.mid(9,2) == "21")
    //    txt2.append(" 5");
    //if (txt2.left(1) == "B")
    //    txt2.append(" 0");

#ifndef STATIC_BUILD
    emit log("TX: " + txt, rxTxLog, false);
#endif

    if (port && port->isOpen()) {
        port->write((txt + '\r').toAscii());
    }
}

void elm327::write(const QByteArray &data)
{
    if (data.length() > 8)
        return;

    QString txt;

    for (int i = 0; i < data.length(); i++) {
        quint8 dat = data.at(i);
        txt += toHex(dat, 2) + " ";
    }
    txt.chop(1);

    write(txt);
}

// need to return a list of CAN messages rather than lumped together.
QList<canFrame*>* elm327::getResponseCAN(int &status)
{
    status = 0;
    QString response = getLine();
    QList<canFrame*>* result = new QList<canFrame*>;
    int triesForPrompt = 20;

    if (response.left(2) == "AT") {
        status |= AT_RESPONSE;
        response = getLine(); // echos must be on, get another line
    }

    if (response.length() == 0) {
        status |= TIMEOUT_ERROR;
    }
    else if (response == "OK") {
        status |= OK_RESPONSE;
        response = getLine();
    }
    else if (response == "STOPPED") {
        status |= STOPPED_RESPONSE;
        response = getLine();
    }
    else if (response == "?") {
        status |= UNKNOWN_RESPONSE;
        response = getLine();
    }
    else if (response == "NO DATA") {
        status |= NO_DATA_RESPONSE;
        response = getLine();
    }
    else if (response == "CAN ERROR") {
        status |= CAN_ERROR;
        response = getLine();
    }

    if (status != 0) {
        triesForPrompt = 1;
    }

    // receive data until prompt appears
    for (int i = 0; response != ">"; i++) {
        if (i >= triesForPrompt) {
            status |= NO_PROMPT_ERROR;
            break;
        }

        if (response == "STOPPED") {
            status |= STOPPED_RESPONSE;
        }
        else {
            canFrame* newCF = hexToCF(response);
            if (!newCF) {
                status |= PROCESSING_ERROR;
                response = getLine();
                continue;
            }
            result->append(newCF);
        }
        response = getLine();
    }

    return result;
}

bool elm327::getResponseStatus(int &status)
{
    status = 0;
    QString response = getLine();
    bool ret = false;

    if (response.left(2) == "AT") {
        status |= AT_RESPONSE;
        response = getLine(); // echos must be on, get another line
    }

    if (response == "OK") {
        status |= OK_RESPONSE;
        ret = true;
    }
    else if (response == "?") {
        status |= UNKNOWN_RESPONSE;
    }

    // receive data until prompt appears
    for (int i = 0; getLine() != ">"; i++) {
        if (i >= 2) {
            status |= NO_PROMPT_ERROR;
            break;
        }
    }

    return ret;
}

void elm327::setSerialParams(const serialSettings &in)
{
    if (port && port->isOpen()) {
        port->close();
        portOpen = false;
        emit portClosed();
    }

    settings = in;
}

bool elm327::getPortOpen()
{
    return portOpen;
}

void elm327::setSendCanID(int id)
{
    sendCanID = id;
    QString idStr = toHex(id, 3);
    write("AT SH " + idStr);
}

void elm327::setRecvCanID(int id)
{
    recvCanID = id;
    QString idStr = toHex(id, 3);
    write("AT CRA " + idStr);
}

QString elm327::getResponseStr(int &status)
{
    status = 0;
    QString response = getLine();

    // receive data until prompt appears
    for (int i = 0; getLine() != ">"; i++) {
        if (i >= 2) {
            status |= NO_PROMPT_ERROR;
            break;
        }
    }

    return response;
}

canFrame* elm327::hexToCF(QString &input)
{
    int id, len, tst = 0;
    QByteArray data;
    bool ok;

    QString orig = input;
    input.remove(' ');

    id = input.mid(0, 3).toUInt(&ok, 16);
    if (!ok) {
        tst++;
        return 0;
    }
    len = input.mid(3, 1).toUInt(&ok, 16);
    if (!ok) {
        tst++;
        return 0;
    }

    QString dataStr = input.mid(4);

    if (dataStr.length() % 2) {
        // not divisable by 2, error in data
        tst++;
        return 0;
    }

    for (int i = 0; i < dataStr.length() / 2; i++) {
        quint8 byte = dataStr.mid(i*2, 2).toUShort(&ok, 16);
        if (!ok) {
            tst++;
            return 0;
        }
        data.append(byte);
    }

    if (data.length() != len) {
        tst++;
        return 0;
    }

    return new canFrame(id, len, data);
}

QString elm327::getLine(int timeout, bool wait)
{
    QString result;
    protectLines->lock();
    if (bufferedLines.empty() && wait) {
        linesAvailable.wait(protectLines, timeout);
    }
    if (!bufferedLines.empty()) {
        result = bufferedLines.takeFirst();
    }
    protectLines->unlock();
    return result;
}

void elm327::constructLine()
{
    static QByteArray data;
    data.append(port->readAll());

    if (data.endsWith('\r') || data.endsWith('>')) {
        QString line(data);

        if (line.endsWith('\n')) {
            line.chop(1);
        }
        if (line.endsWith('\r')) {
            line.chop(1);
        }

        data.clear();

        if (line.isEmpty()) {
            return;
        }

#ifndef STATIC_BUILD
        emit log("RX: " + line, rxTxLog, false);
#endif

        protectLines->lock();
        bufferedLines << line;
        linesAvailable.wakeAll();
        protectLines->unlock();
    }
}
