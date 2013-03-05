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
