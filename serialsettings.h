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

#ifndef SERIALSETTINGS_H
#define SERIALSETTINGS_H

#include <QDialog>
#include <QIntValidator>

#include "serialport.h"
#include "serialportinfo.h"
using namespace QtAddOn::SerialPort;

struct serialSettings {
    QString name;
    qint32 rate;
    SerialPort::DataBits dataBits;
    SerialPort::Parity parity;
    SerialPort::StopBits stopBits;
    SerialPort::FlowControl flowControl;
};

namespace Ui {
class serialSettingsDialog;
}

class serialSettingsDialog : public QDialog
{
    Q_OBJECT
public:
    explicit serialSettingsDialog(QWidget *parent = 0);
    ~serialSettingsDialog();
    void showEvent(QShowEvent *event);
    void hideEvent(QHideEvent *event);
    serialSettings getSettings() const;
    void setSettings(const serialSettings &newSettings);
signals:
    void settingsApplied();
private slots:
    void showPortInfo(int idx);
    void saveAndHide();
    void cancelAndHide();
    void checkCustomRatePolicy(int idx);
    void on_pushbutton_refresh_clicked();

private:
    void fillPortsParameters();
    void fillPortsInfo();
    void updateSettings();
private:
    Ui::serialSettingsDialog *ui;
    serialSettings currentSettings;
    QIntValidator *intValidator;
};

#endif // SERIALSETTINGS_H
