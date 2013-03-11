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

#include "monitor.h"

#include <QCoreApplication>

monitor::monitor(elm327 *elm, QObject *parent) :
    QObject(parent),
    elm(elm),
    filename("canLog.txt"),
    outFile(0),
    monitoring(false)
{
}

void monitor::start()
{
    outFile = new QFile(filename, this);
    outFile->open(QIODevice::WriteOnly);
    out.setDevice(outFile);

    QString line;
    monitoring = true;
    int timeoutCount = 0;

    elm->write(QString("ATMA"));

    while (monitoring || timeoutCount >= 5) {
        line = elm->getLine();
        if (line == "") {
            timeoutCount++;
        }

        if (line == ">") {
            monitoring = false;
        }

        out << line << endl;

        //QCoreApplication::processEvents();
    }

    out.flush();
    out.setDevice(0);
    outFile->close();
    delete outFile;
    outFile = 0;

    emit done();
}

void monitor::stop()
{
    monitoring = false;
    elm->write(QString("."));
}
