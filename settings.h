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

#ifndef SETTINGS_H
#define SETTINGS_H

#include <QDialog>
#include <QSettings>
#include <QFileDialog>
#include <QIntValidator>

namespace Ui {
class settings;
}

class settings : public QDialog
{
    Q_OBJECT

public:
    explicit settings(QSettings* appSettings, QWidget *parent = 0);
    ~settings();

    void showEvent(QShowEvent *event);
    
    int slow, norm, fast;
    int keepAliveInterval;
    int rate;
    int historySecs;
    QString labelDir;

    void load();
    void save();

private slots:
    void on_buttonBox_accepted();
    void on_buttonBox_rejected();

    void on_pushButton_clicked();

private:
    Ui::settings *ui;
    QSettings* appSettings;

    QIntValidator receiveTimeValidator;
    QIntValidator rateValidator;
    QIntValidator keepAliveValidator;
    QIntValidator historyValidator;
signals:
    void settingsChanged();
};

#endif // SETTINGS_H
