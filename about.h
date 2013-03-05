#ifndef ABOUT_H
#define ABOUT_H

#include <QDialog>

namespace Ui {
class about;
}

class about : public QDialog
{
    Q_OBJECT
public:
    explicit about(QString version, QString svn, QWidget *parent = 0);
    ~about();
    
private:
    Ui::about *ui;
private slots:

};

#endif // ABOUT_H
