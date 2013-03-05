#include "serialsettings.h"
#include "ui_serialsettings.h"

#include <QLineEdit>

serialSettingsDialog::serialSettingsDialog(QWidget *parent) :
    QDialog(parent),
    ui(new Ui::serialSettingsDialog)
{
    ui->setupUi(this);

    intValidator = new QIntValidator(0, 4000000, this);

    ui->rateBox->setInsertPolicy(QComboBox::NoInsert);

    connect(ui->buttonBox, SIGNAL(accepted()), this, SLOT(saveAndHide()));
    connect(ui->buttonBox, SIGNAL(rejected()), this, SLOT(cancelAndHide()));
    connect(ui->portsBox, SIGNAL(currentIndexChanged(int)), this, SLOT(showPortInfo(int)));
    connect(ui->rateBox, SIGNAL(currentIndexChanged(int)), this, SLOT(checkCustomRatePolicy(int)));

    fillPortsParameters();

    updateSettings();
}

serialSettingsDialog::~serialSettingsDialog()
{
    delete ui;
}

void serialSettingsDialog::showEvent(QShowEvent *event)
{
    fillPortsInfo();
}

void serialSettingsDialog::hideEvent(QHideEvent *event)
{
    setSettings(currentSettings);
}

serialSettings serialSettingsDialog::getSettings() const
{
    return currentSettings;
}

void serialSettingsDialog::showPortInfo(int idx)
{
    if (idx != -1) {
        QStringList list = ui->portsBox->itemData(idx).toStringList();
        ui->descriptionLabel->setText(tr("Description: %1").arg(list.at(1)));
        ui->manufacturerLabel->setText(tr("Manufacturer: %1").arg(list.at(2)));
        ui->locationLabel->setText(tr("Location: %1").arg(list.at(3)));
        ui->vidLabel->setText(tr("Vendor Identifier: %1").arg(list.at(4)));
        ui->pidLabel->setText(tr("Product Identifier: %1").arg(list.at(5)));
    }
}

void serialSettingsDialog::saveAndHide()
{
    updateSettings();
    emit settingsApplied();
    hide();
}

void serialSettingsDialog::cancelAndHide()
{
    hide();
}

void serialSettingsDialog::checkCustomRatePolicy(int idx)
{
    ui->rateBox->setEditable(idx == 4);
    if (idx == 4) {
        ui->rateBox->clearEditText();
        QLineEdit *edit = ui->rateBox->lineEdit();
        edit->setValidator(intValidator);
    }
}

void serialSettingsDialog::fillPortsParameters()
{
    // fill baud rate (is not the entire list of available values,
    // desired values??, add your independently)
    ui->rateBox->addItem(QLatin1String("9600"), SerialPort::Rate9600);
    ui->rateBox->addItem(QLatin1String("19200"), SerialPort::Rate19200);
    ui->rateBox->addItem(QLatin1String("38400"), SerialPort::Rate38400);
    ui->rateBox->addItem(QLatin1String("115200"), SerialPort::Rate115200);
    ui->rateBox->addItem(QLatin1String("Custom"));

    // fill data bits
    ui->dataBitsBox->addItem(QLatin1String("5"), SerialPort::Data5);
    ui->dataBitsBox->addItem(QLatin1String("6"), SerialPort::Data6);
    ui->dataBitsBox->addItem(QLatin1String("7"), SerialPort::Data7);
    ui->dataBitsBox->addItem(QLatin1String("8"), SerialPort::Data8);
    ui->dataBitsBox->setCurrentIndex(3);

    // fill parity
    ui->parityBox->addItem(QLatin1String("None"), SerialPort::NoParity);
    ui->parityBox->addItem(QLatin1String("Even"), SerialPort::EvenParity);
    ui->parityBox->addItem(QLatin1String("Odd"), SerialPort::OddParity);
    ui->parityBox->addItem(QLatin1String("Mark"), SerialPort::MarkParity);
    ui->parityBox->addItem(QLatin1String("Space"), SerialPort::SpaceParity);

    // fill stop bits
    ui->stopBitsBox->addItem(QLatin1String("1"), SerialPort::OneStop);
#ifdef Q_OS_WIN
    ui->stopBitsBox->addItem(QLatin1String("1.5"), SerialPort::OneAndHalfStop);
#endif
    ui->stopBitsBox->addItem(QLatin1String("2"), SerialPort::TwoStop);

    // fill flow control
    ui->flowControlBox->addItem(QLatin1String("None"), SerialPort::NoFlowControl);
    ui->flowControlBox->addItem(QLatin1String("RTS/CTS"), SerialPort::HardwareControl);
    ui->flowControlBox->addItem(QLatin1String("XON/XOFF"), SerialPort::SoftwareControl);
}

void serialSettingsDialog::fillPortsInfo()
{
    ui->portsBox->clear();
    foreach (const SerialPortInfo &info, SerialPortInfo::availablePorts()) {
        QStringList list;
        list << info.portName() << info.description()
             << info.manufacturer() << info.systemLocation()
             << info.vendorIdentifier() << info.productIdentifier();

        ui->portsBox->addItem(list.at(0), list);
    }
}

void serialSettingsDialog::updateSettings()
{
    currentSettings.name = ui->portsBox->currentText();

    // Rate
    if (ui->rateBox->currentIndex() == 4) {
        // custom rate
        currentSettings.rate = ui->rateBox->currentText().toInt();
    } else {
        // standard rate
        currentSettings.rate = static_cast<SerialPort::Rate>(
                    ui->rateBox->itemData(ui->rateBox->currentIndex()).toInt());
    }

    // Data bits
    currentSettings.dataBits = static_cast<SerialPort::DataBits>(
                ui->dataBitsBox->itemData(ui->dataBitsBox->currentIndex()).toInt());

    // Parity
    currentSettings.parity = static_cast<SerialPort::Parity>(
                ui->parityBox->itemData(ui->parityBox->currentIndex()).toInt());

    // Stop bits
    currentSettings.stopBits = static_cast<SerialPort::StopBits>(
                ui->stopBitsBox->itemData(ui->stopBitsBox->currentIndex()).toInt());

    // Flow control
    currentSettings.flowControl = static_cast<SerialPort::FlowControl>(
                ui->flowControlBox->itemData(ui->flowControlBox->currentIndex()).toInt());
}

void serialSettingsDialog::setSettings(const serialSettings &newSettings)
{
    currentSettings = newSettings;

    int pos = ui->portsBox->findText(currentSettings.name, Qt::MatchExactly);
    if (pos >= 0) {
        ui->portsBox->setCurrentIndex(pos);
    }

    pos = ui->rateBox->findData(currentSettings.rate, Qt::UserRole, Qt::MatchExactly);
    if (pos >= 0) {
        ui->rateBox->setCurrentIndex(pos);
    }
    else {
        ui->rateBox->setCurrentIndex(4);
        QLineEdit* tmp = ui->rateBox->lineEdit();
        tmp->setText(QString::number(currentSettings.rate));
    }

    pos = ui->dataBitsBox->findData(currentSettings.dataBits, Qt::UserRole, Qt::MatchExactly);
    if (pos >= 0) {
        ui->dataBitsBox->setCurrentIndex(pos);
    }

    pos = ui->parityBox->findData(currentSettings.parity, Qt::UserRole, Qt::MatchExactly);
    if (pos >= 0) {
        ui->parityBox->setCurrentIndex(pos);
    }

    pos = ui->stopBitsBox->findData(currentSettings.stopBits, Qt::UserRole, Qt::MatchExactly);
    if (pos >= 0) {
        ui->stopBitsBox->setCurrentIndex(pos);
    }

    pos = ui->flowControlBox->findData(currentSettings.flowControl, Qt::UserRole, Qt::MatchExactly);
    if (pos >= 0) {
        ui->flowControlBox->setCurrentIndex(pos);
    }
}

void serialSettingsDialog::on_pushbutton_refresh_clicked()
{
    fillPortsInfo();
}
