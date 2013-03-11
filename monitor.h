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

#ifndef MONITOR_H
#define MONITOR_H

#include <QObject>
#include <QFile>
#include <QTextStream>

#include "elm327.h"

class monitor : public QObject
{
    Q_OBJECT
public:
    explicit monitor(elm327* elm, QObject *parent = 0);
    
signals:
    void done();
public slots:
    void start();
    void stop();
private:
    elm327* elm;

    QString filename;
    QFile* outFile;
    QTextStream out;

    bool monitoring;
};

#endif // MONITOR_H
