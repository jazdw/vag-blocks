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

#include "about.h"
#include "ui_about.h"

about::about(QString version, QString svn, QWidget *parent) :
    QDialog(parent),
    ui(new Ui::about)
{
    ui->setupUi(this);

    ui->label->setText(ui->label->text().replace("%VER%", version));
    ui->label->setText(ui->label->text().replace("%REV%", svn));
}

about::~about()
{
    delete ui;
}
