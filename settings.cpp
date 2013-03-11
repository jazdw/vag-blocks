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

#include "settings.h"
#include "ui_settings.h"
#include "mainwindow.h"

settings::settings(QSettings* appSettings, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::settings),
    appSettings(appSettings),
    receiveTimeValidator(1, 255, this),
    rateValidator(1, 50, this),
    keepAliveValidator(1, 10000, this),
    historyValidator(1, 300, this)
{
    ui->setupUi(this);

    // TODO: Fix validators allowing 0
    ui->lineEdit_slow->setValidator(&receiveTimeValidator);
    ui->lineEdit_normal->setValidator(&receiveTimeValidator);
    ui->lineEdit_fast->setValidator(&receiveTimeValidator);
    ui->lineEdit_rate->setValidator(&rateValidator);
    ui->lineEdit_keepAliveInterval->setValidator(&keepAliveValidator);
    ui->lineEdit_history->setValidator(&historyValidator);
}

settings::~settings()
{
    delete ui;
}

void settings::showEvent(QShowEvent* event)
{
    ui->lineEdit_slow->setText(QString::number(slow));
    ui->lineEdit_normal->setText(QString::number(norm));
    ui->lineEdit_fast->setText(QString::number(fast));

    ui->lineEdit_keepAliveInterval->setText(QString::number(keepAliveInterval));

    ui->lineEdit_rate->setText(QString::number(rate));
    ui->lineEdit_history->setText(QString::number(historySecs));

    ui->lineEdit_labelDir->setText(labelDir);
}

void settings::on_buttonBox_accepted()
{
    slow = ui->lineEdit_slow->text().toUInt();
    norm = ui->lineEdit_normal->text().toUInt();
    fast = ui->lineEdit_fast->text().toUInt();
    keepAliveInterval = ui->lineEdit_keepAliveInterval->text().toUInt();
    rate = ui->lineEdit_rate->text().toUInt();
    historySecs = ui->lineEdit_history->text().toUInt();
    labelDir = ui->lineEdit_labelDir->text();

    save();

    emit settingsChanged();
}

void settings::on_buttonBox_rejected()
{

}

void settings::load()
{
    labelDir = appSettings->value("Labels/dir", QString("Labels")).toString();

    rate = appSettings->value("Plot/updateRate", 10).toUInt();
    if (rate > rateValidator.top() || rate < rateValidator.bottom()) {
        rate = 10;
    }

    historySecs = appSettings->value("Plot/secondsOfHistory", 60).toUInt();
    if (historySecs > historyValidator.top() || historySecs < historyValidator.bottom()) {
        historySecs = 60;
    }

    slow = appSettings->value("Timeouts/slow", 40).toInt();
    norm = appSettings->value("Timeouts/norm", 24).toInt();
    fast = appSettings->value("Timeouts/fast", 16).toInt();

    keepAliveInterval = appSettings->value("KeepAlive/interval", 800).toInt();
}

void settings::save()
{
    appSettings->setValue("Labels/dir", labelDir);

    appSettings->setValue("Plot/updateRate", rate);
    appSettings->setValue("Plot/secondsOfHistory", historySecs);

    appSettings->setValue("Timeouts/slow", slow);
    appSettings->setValue("Timeouts/norm", norm);
    appSettings->setValue("Timeouts/fast", fast);

    appSettings->setValue("KeepAlive/interval", keepAliveInterval);

    appSettings->sync();
}

void settings::on_pushButton_clicked()
{
    QString dirStr = QFileDialog::getExistingDirectory(this, "Select a directory containing label files");
    ui->lineEdit_labelDir->setText(dirStr);
}
