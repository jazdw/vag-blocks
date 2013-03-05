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
