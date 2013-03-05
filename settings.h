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
