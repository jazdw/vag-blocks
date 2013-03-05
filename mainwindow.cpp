#include "mainwindow.h"
#include "ui_mainwindow.h"

#include <QFileInfo>
#include <QDir>
#include "util.h"

MainWindow::MainWindow(QWidget *parent) :
    QMainWindow(parent),
    ui(new Ui::MainWindow),
    kwp(this),
    appSettings(new QSettings("vagblocks.ini", QSettings::IniFormat, this)),
    serialConfigured(false),
    storedRow(-1), storedCol(-1),
    currentlyLogging(false)
{
    /*
    QFile roboto(":/resources/Roboto-Regular.ttf");
    roboto.open(QIODevice::ReadOnly);
    if (QFontDatabase::addApplicationFontFromData(roboto.readAll()) < 0) {
        // error
    }
    roboto.close();
    */

    ui->setupUi(this);

    QString svnRev = APP_SVN_REV;
    int colonAt = svnRev.indexOf(":");
    if (colonAt > 0) {
        svnRev = svnRev.mid(colonAt+1);
    }
    QString appVer = APP_VERSION;
    aboutDialog = new about(appVer, svnRev, this);
    serSettings = new serialSettingsDialog(this);
    settingsDialog = new settings(appSettings, this);

    connect(ui->action_About, SIGNAL(triggered()), aboutDialog, SLOT(show()));
    connect(ui->actionApplication_settings, SIGNAL(triggered()), settingsDialog, SLOT(show()));

    setupBlockArray(ui->blocksLayout);

    connect(&kwp, SIGNAL(log(QString, int)), this, SLOT(log(QString, int)));
    connect(&kwp, SIGNAL(newBlockData(int)), this, SLOT(newBlockData(int)));
    connect(&kwp, SIGNAL(blockOpen(int)), this, SLOT(blockOpen(int)));
    connect(&kwp, SIGNAL(blockClosed(int)), this, SLOT(blockClosed(int)));
    connect(&kwp, SIGNAL(channelOpen(bool)), this, SLOT(channelOpen(bool)));
    connect(&kwp, SIGNAL(elmInitialised(bool)), this, SLOT(elmInitialised(bool)));
    connect(&kwp, SIGNAL(portOpened(bool)), this, SLOT(portOpened(bool)));
    connect(&kwp, SIGNAL(portClosed()), this, SLOT(portClosed()));
    connect(&kwp, SIGNAL(newModuleInfo(QStringList)), this, SLOT(moduleInfoReceived(QStringList)));
    connect(&kwp, SIGNAL(newEcuInfo(QStringList)), this, SLOT(ecuInfoReceived(QStringList)));
    connect(ui->pushButton_log, SIGNAL(clicked(bool)), this, SLOT(startLogging(bool)));
    connect(&kwp, SIGNAL(labelsLoaded(bool)), this, SLOT(labelsLoaded(bool)));
    connect(ui->lineEdit_moduleNum, SIGNAL(textChanged(QString)), this, SLOT(clearUI()));
    connect(ui->comboBox_modules, SIGNAL(activated(int)), this, SLOT(selectNewModule(int)));
    connect(&kwp, SIGNAL(moduleListRefreshed()), this, SLOT(refreshModules()));
    connect(ui->pushButton_refresh, SIGNAL(clicked()), &kwp, SLOT(openGW_refresh()));
    connect(&kwp, SIGNAL(sampleFormatChanged()), this, SLOT(sampleFormatChanged()));
    connect(&kwp, SIGNAL(loggingStarted()), this, SLOT(loggingStarted()));
    connect(settingsDialog, SIGNAL(settingsChanged()), this, SLOT(updateSettings()));

    for (int i = 0; i < 16; i++) { // setup running average for sample rate
        avgList.append(0);
    }

    restoreSettings();

    if (serialConfigured) {
        kwp.setSerialParams(serSettings->getSettings());
        QMetaObject::invokeMethod(&kwp, "openPort", Qt::QueuedConnection);
    }

    connect(serSettings, SIGNAL(settingsApplied()), this, SLOT(connectToSerial()));
    refreshModules(true);
}

MainWindow::~MainWindow()
{
    kwp.closePortBlocking();

    delete ui;
}

void MainWindow::closeEvent(QCloseEvent *event)
{
    event++; // get rid of warning
    saveDocks();
    serSettings->hide();
    aboutDialog->hide();
    settingsDialog->hide();
    ui->dockWidget_log->setVisible(false);
    ui->dockWidget_module->setVisible(false);
    ui->dockWidget_info->setVisible(false);
    ui->dockWidget_plot->setVisible(false);
    saveSettings();
}

void MainWindow::hideEvent(QHideEvent *event)
{
    event++; // get rid of warning
    //saveDocks();
}

void MainWindow::changeEvent(QEvent *event)
{
    if (event->type() == QEvent::WindowStateChange) {
        QWindowStateChangeEvent* eventWindowChange = static_cast<QWindowStateChangeEvent*>(event);
        if (eventWindowChange->oldState() == Qt::WindowMinimized) {
            //restoreDocks();
        }
    }
}

void MainWindow::saveDocks()
{
    showLogDock = ui->dockWidget_log->isVisible();
    showModuleDock = ui->dockWidget_module->isVisible();
    showInfoDock = ui->dockWidget_info->isVisible();
    showPlotDock = ui->dockWidget_plot->isVisible();
}

void MainWindow::restoreDocks()
{
    ui->dockWidget_log->setVisible(showLogDock);
    ui->dockWidget_module->setVisible(showModuleDock);
    ui->dockWidget_info->setVisible(showInfoDock);
    ui->dockWidget_plot->setVisible(showPlotDock);

    ui->actionShow_log_dock->setChecked(ui->dockWidget_log->isVisible());
    ui->actionShow_Info_dock->setChecked(ui->dockWidget_module->isVisible());
    ui->actionShow_Module_dock->setChecked(ui->dockWidget_info->isVisible());
    ui->actionShow_Plot_dock->setChecked(ui->dockWidget_plot->isVisible());
}

// Serial settings and window/dock settings handled here
// Other user configurable values are set in settingsDialog
void MainWindow::saveSettings()
{
    appSettings->setValue("Serial/configured", serialConfigured);
    serialSettings tmp = serSettings->getSettings();
    appSettings->setValue("Serial/portName", tmp.name);
    appSettings->setValue("Serial/rate", tmp.rate);
    appSettings->setValue("Serial/dataBits", tmp.dataBits);
    appSettings->setValue("Serial/parity", tmp.parity);
    appSettings->setValue("Serial/stopBits", tmp.stopBits);
    appSettings->setValue("Serial/flowControl", tmp.flowControl);

    appSettings->setValue("Log/logLevel", logLevel);

    settingsDialog->save();

    appSettings->setValue("DockVisibility/log", showLogDock);
    appSettings->setValue("DockVisibility/module", showModuleDock);
    appSettings->setValue("DockVisibility/info", showInfoDock);
    appSettings->setValue("DockVisibility/plot", showPlotDock);

    appSettings->setValue("MainWindow/state", saveState());
    appSettings->setValue("MainWindow/geometry", saveGeometry());

    appSettings->sync();
}

// Serial settings and window/dock settings handled here
// Other user configurable values are set in settingsDialog
void MainWindow::restoreSettings()
{
    serialConfigured = appSettings->value("Serial/configured", false).toBool();
    serialSettings tmp;
    bool ok;
    tmp.name = appSettings->value("Serial/portName", QString("COM1")).toString();
    tmp.rate = appSettings->value("Serial/rate", SerialPort::Rate115200).toInt(&ok);
    tmp.dataBits = static_cast<SerialPort::DataBits>(appSettings->value("Serial/dataBits", SerialPort::Data8).toInt(&ok));
    tmp.parity = static_cast<SerialPort::Parity>(appSettings->value("Serial/parity", SerialPort::NoParity).toInt(&ok));
    tmp.stopBits = static_cast<SerialPort::StopBits>(appSettings->value("Serial/stopBits", SerialPort::OneStop).toInt(&ok));
    tmp.flowControl = static_cast<SerialPort::FlowControl>(appSettings->value("Serial/flowControl", SerialPort::NoFlowControl).toInt(&ok));
    serSettings->setSettings(tmp);

    logLevel = appSettings->value("Log/logLevel", stdLog | serialConfigLog).toInt();

    settingsDialog->load();
    updateSettings();

    restoreGeometry(appSettings->value("MainWindow/geometry").toByteArray());
    restoreState(appSettings->value("MainWindow/state").toByteArray());

    showLogDock = appSettings->value("DockVisibility/log", true).toBool();
    showModuleDock = appSettings->value("DockVisibility/module", true).toBool();
    showInfoDock = appSettings->value("DockVisibility/info", true).toBool();
    showPlotDock = appSettings->value("DockVisibility/plot", true).toBool();
    restoreDocks();
}

void MainWindow::setupBlockArray(QVBoxLayout *in)
{
    ui->label_selectedTitle->setText("Select a value");
    ui->label_selectedSub->clear();
    ui->label_selectedLong->clear();
    ui->label_selectedValue->clear();
    ui->label_selectedUnits->clear();
    ui->label_selectedBinary->clear();
    ui->label_selectedSub->hide();
    ui->label_selectedLong->hide();
    ui->label_selectedValue->hide();
    ui->label_selectedUnits->hide();
    ui->label_selectedBinary->hide();

    connect(&mapButtons, SIGNAL(mapped(int)), this, SLOT(openCloseBlock(int)));
    connect(&mapValueClick, SIGNAL(mapped(int)), this, SLOT(displayInfo(int)));

    for (int i = 0; i < 4; i++) {
        QGridLayout* row = static_cast<QGridLayout*>(in->itemAt(i)->layout());

        blockDisplays[i].pushButton = static_cast<QPushButton*>(row->itemAtPosition(3,0)->widget());
        mapButtons.setMapping(blockDisplays[i].pushButton, i);
        connect(blockDisplays[i].pushButton, SIGNAL(clicked()), &mapButtons, SLOT(map()));

        blockDisplays[i].spinBox = static_cast<QSpinBox*>(row->itemAtPosition(3,1)->widget());
        connect(blockDisplays[i].spinBox, SIGNAL(valueChanged(int)), this, SLOT(spinChanged(int)));

        blockDisplays[i].blockTitle = static_cast<QLabel*>(row->itemAtPosition(1,0)->widget());

        for (int j = 0; j < 4; j++) {
            blockDisplays[i].desc[j] = static_cast<QLabel*>(row->itemAtPosition(1,j+2)->widget());
            blockDisplays[i].subDesc[j] = static_cast<QLabel*>(row->itemAtPosition(2,j+2)->widget());
            blockDisplays[i].lineEdit[j] = static_cast<clickLineEdit*>(row->itemAtPosition(3,j+2)->widget());

            mapValueClick.setMapping(blockDisplays[i].lineEdit[j], i*4+j);
            connect(blockDisplays[i].lineEdit[j], SIGNAL(clicked()), &mapValueClick, SLOT(map()));

            blockDisplays[i].desc[j]->setText("Value " + QString::number(j+1));
            //blockDisplays[i].subDesc[j]->setText("");
        }

        blockDisplays[i].blockNum = -1;
    }
}

int MainWindow::getBlockRow(int blockNum)
{
    int row = -1;
    for (int i = 0; i < 4; i++) {
        if (blockDisplays[i].blockNum == blockNum) {
            row = i;
            break;
        }
    }
    return row;
}

void MainWindow::log(const QString &txt, int logLevel)
{
    if (logLevel & this->logLevel) {
        logBuffer << txt;
        if (logLevel != rxTxLog || logBuffer.length() >= 50) {
            flushLogBuffer();
        }
    }
}

void MainWindow::flushLogBuffer()
{
    if (!logBuffer.empty()) {
        ui->plainTextEdit_log->appendPlainText(logBuffer.join("\n"));
        logBuffer.clear();
    }
}

void MainWindow::plotUpdate()
{
    int numSamples = settingsDialog->rate * settingsDialog->historySecs + 1;
    if (curves.size() > 0) {
        QList<sampleValue> sampleList = kwp.getSample();

        for (int i = 0; i < curves.size(); i++) {
            QVariant val = sampleList[i].val;
            QMetaType::Type valType = static_cast<QMetaType::Type>(val.type());
            if (valType == QMetaType::Double || valType == QMetaType::UInt || valType == QMetaType::Int) {
                curves[i].data->append(sampleList[i].val.toDouble());
            }
            else {
                curves[i].data->append(0);
            }
            curves[i].data->remove(0,1);
            curves[i].curve->setRawSamples(timeAxis.constData(), curves[i].data->constData(), numSamples);
            curves[i].curve->attach(ui->plot);
        }
        ui->plot->replot();
    }
}

void MainWindow::sampleFormatChanged()
{
    for (int i = 0; i < curves.size(); i++) {
        curves[i].curve->detach();
        delete curves[i].curve;
        curves[i].data->clear();
        delete curves[i].data;
    }
    curves.clear();
    timeAxis.clear();

    QList<sampleValue> sampleList = kwp.getSample();
    int numSamples = settingsDialog->rate * settingsDialog->historySecs + 1;

    timeAxis.clear();
    for (int i = 0; i < numSamples; i++) {
        timeAxis.prepend(-i / static_cast<double>(settingsDialog->rate));
    }

    int numColors = sizeof(colorList) / sizeof(QString);

    for (int i = 0; i < sampleList.length(); i++) {
        QwtPlotCurve* curve = new QwtPlotCurve;
        QVector<double>* data = new QVector<double>();
        data->reserve(numSamples+1);
        data->fill(0, numSamples);

        QColor curveColor;
        curveColor.setNamedColor(colorList[i % numColors]);

        int blockNum = sampleList.at(i).refs.at(0).blockNum;
        int pos = sampleList.at(i).refs.at(0).pos;
        blockLabels_t label = kwp.getBlockLabel(blockNum);
        QString desc = label.desc[pos] + " " + label.subDesc[pos];

        curve->setRenderHint(QwtPlotItem::RenderAntialiased);
        curve->setPen(curveColor);
        curve->setTitle(desc);

        curve->setRawSamples(timeAxis.constData(), data->constData(), numSamples);
        curve->attach(ui->plot);

        // set legend checked
        QwtLegendItem *legendItem = qobject_cast<QwtLegendItem*>(ui->plot->legend()->find(curve));
        if (legendItem) {
            legendItem->setChecked(true);
        }

        curveAndData tmp;
        tmp.curve = curve;
        tmp.data = data;
        curves.append(tmp);
    }
}

void MainWindow::setupPlot()
{
    plotTimer.setInterval(1000/settingsDialog->rate);
    connect(&plotTimer, SIGNAL(timeout()), this, SLOT(plotUpdate()));
    plotTimer.start();

    if (!ui->plot->legend()) {
        QwtLegend* legend = new QwtLegend;
        legend->setItemMode(QwtLegend::CheckableItem);
        ui->plot->insertLegend(legend, QwtPlot::RightLegend);
    }

    ui->plot->setAxisTitle(QwtPlot::xBottom, "seconds");
    ui->plot->setCanvasBackground(Qt::white);

    QColor gridColor;
    gridColor.setNamedColor("grey");
    QPen gridPen(gridColor);
    gridPen.setStyle(Qt::DotLine);
    QwtPlotGrid* grid = new QwtPlotGrid;
    grid->setMajPen(gridPen);
    grid->attach(ui->plot);

    connect(ui->plot, SIGNAL(legendChecked(QwtPlotItem*,bool)), this, SLOT(showCurve(QwtPlotItem*,bool)));
}

void MainWindow::showCurve(QwtPlotItem *item, bool on)
{
    item->setVisible(on);

    QwtLegendItem *legendItem = qobject_cast<QwtLegendItem*>(ui->plot->legend()->find(item));

    if (legendItem) {
        legendItem->setChecked(on);
    }
}

void MainWindow::loggingStarted()
{
    QFileInfo logInfo = kwp.getLogfileInfo();
    QString link = "<a href=\"file:///" + logInfo.absoluteFilePath() + "\">";
    link += logInfo.fileName() + "</a>";

    ui->label_logLink->setText(link);
}

void MainWindow::updateSettings()
{
    setupPlot();
    kwp.setLabelDir(QDir::fromNativeSeparators(settingsDialog->labelDir));
    kwp.setTimeouts(settingsDialog->slow, settingsDialog->norm, settingsDialog->fast);
    kwp.setKeepAliveInterval(settingsDialog->keepAliveInterval);
}

void MainWindow::newBlockData(int blockNum)
{
    int row = getBlockRow(blockNum);
    if (row == -1) {
        QMetaObject::invokeMethod(&kwp, "closeBlock", Qt::QueuedConnection,
                                  Q_ARG(int, blockNum));
        return;
    }

    // Update UI with new values and units
    for (int pos = 0; pos < 4; pos++) {
        QString units = kwp.getBlockUnits(blockNum, pos);
        QVariant value = kwp.getBlockValue(blockNum, pos);

        bool ok;
        if (static_cast<QMetaType::Type>(value.type()) == QMetaType::Double) {
            double tmp = value.toDouble();
            tmp = static_cast<double>(static_cast<int>(tmp*100+0.5))/100.0;
            QString formatted = QString::number(tmp);

            if (units.at(0) == QChar(0x00B0)) {
                blockDisplays[row].lineEdit[pos]->setText(formatted + units);
            }
            else {
                blockDisplays[row].lineEdit[pos]->setText(formatted + " " + units);
            }

            if (row == storedRow && pos == storedCol) {
                ui->label_selectedValue->setText(blockDisplays[row].lineEdit[pos]->text());
                ui->label_selectedUnits->clear();
                ui->label_selectedUnits->hide();
            }
        }
        if (static_cast<QMetaType::Type>(value.type()) == QMetaType::UInt) {
            unsigned int raw = value.toUInt(&ok);
            blockDisplays[row].lineEdit[pos]->setText("0x" + toHex(raw, 4));

            if (row == storedRow && pos == storedCol) {
                if (units == "Binary") {
                    ui->label_selectedValue->setText(uintToBinary(raw & 0x00FF, 8));
                }
                else {
                    ui->label_selectedValue->setText(blockDisplays[row].lineEdit[pos]->text());
                }
                ui->label_selectedUnits->setText(units);
            }
        }
        else if (static_cast<QMetaType::Type>(value.type()) == QMetaType::QString) {
            blockDisplays[row].lineEdit[pos]->setText(value.toString());

            if (row == storedRow && pos == storedCol) {
                ui->label_selectedValue->setText(blockDisplays[row].lineEdit[pos]->text());
                ui->label_selectedUnits->setText(units);
            }
        }
    }

    quint64 time = perfTimer[row].elapsed();
    blockDisplays[row].rate = 1.0/(time/1000.0);

    avgList.removeFirst();
    avgList.append(blockDisplays[row].rate);
    double runningAvg = 0;
    for (int i = 0; i < 16; i++) {
        runningAvg += avgList.at(i);
    }
    runningAvg /= 16;

    ui->lcdNumber->display(runningAvg);
    perfTimer[row].start();
}

void MainWindow::connectToSerial()
{
    saveSettings();
    serialConfigured = true;
    kwp.setSerialParams(serSettings->getSettings());
    QMetaObject::invokeMethod(&kwp, "openPort", Qt::QueuedConnection);
}

void MainWindow::selectNewModule(int pos)
{
    if (pos == ui->comboBox_modules->findText("Other")) {
        ui->lineEdit_moduleNum->setText("");
        ui->lineEdit_moduleNum->setCursorPosition(0);
        ui->lineEdit_moduleNum->setFocus();
        return;
    }

    int moduleNum = ui->comboBox_modules->itemData(pos).toInt();
    ui->lineEdit_moduleNum->setText(QString("%1").arg(moduleNum, 2, 16, QChar('0')));

    if (kwp.getPortOpen() && kwp.getElmInitialised() && kwp.getChannelDest() < 0) {
        QMetaObject::invokeMethod(&kwp, "openChannel", Qt::QueuedConnection,
                                  Q_ARG(int, moduleNum));
    }
}

void MainWindow::on_lineEdit_moduleNum_textChanged(const QString &arg1)
{
    bool ok;
    int moduleNum = arg1.toInt(&ok, 16);
    if (!ok) {
        return;
    }
    if (moduleNum == 0) {
        return;
    }

    int index = ui->comboBox_modules->findData(moduleNum);
    if (index < 0) {
        index = ui->comboBox_modules->findText("Other");
    }
    ui->comboBox_modules->setCurrentIndex(index);
}

void MainWindow::elmInitialised(bool ok)
{
    if (ok) {
        ui->statusBar->showMessage("ELM327 initialisation complete.");
    }
    else {
        ui->statusBar->showMessage("ELM327 initialisation failed.");
        serSettings->show();
    }
}

void MainWindow::portOpened(bool open)
{
    if (open) {
        ui->statusBar->showMessage("Serial port opened.");
    }
    else {
        ui->statusBar->showMessage("Serial port could not be opened.");
        serSettings->show();
    }
}

void MainWindow::portClosed()
{
    ui->statusBar->showMessage("Serial port closed.");
}

void MainWindow::on_pushButton_openModule_clicked()
{
    if (kwp.getChannelDest() >= 0) {
        QMetaObject::invokeMethod(&kwp, "closeChannel", Qt::QueuedConnection);
    }
    else {
        if (ui->lineEdit_moduleNum->text() == "") {
            return;
        }
        bool ok;
        int moduleNum = ui->lineEdit_moduleNum->text().toInt(&ok, 16);
        if (moduleNum == 0) {
            return;
        }
        if (kwp.getPortOpen() && kwp.getElmInitialised()) {
            QMetaObject::invokeMethod(&kwp, "openChannel", Qt::QueuedConnection,
                                      Q_ARG(int, moduleNum));
        }
    }
}

void MainWindow::openCloseBlock(int row)
{
    clearRow(row, false);

    if (kwp.getChannelDest() < 0)
        return;

    int blockNum = blockDisplays[row].spinBox->value();

    blockLabels_t label = kwp.getBlockLabel(blockNum);
    blockDisplays[row].blockTitle->setText(label.blockName);
    blockDisplays[row].spinBox->setToolTip(label.blockName);
    for (int j = 0; j < 4; j++) {
        blockDisplays[row].desc[j]->setText(label.desc[j]);
        blockDisplays[row].subDesc[j]->setText(label.subDesc[j]);
        blockDisplays[row].desc[j]->setToolTip(label.longDesc[j]);
        blockDisplays[row].subDesc[j]->setToolTip(label.longDesc[j]);
        blockDisplays[row].lineEdit[j]->setToolTip(label.longDesc[j]);
    }

    if (blockDisplays[row].blockNum == -1) { // block is closed, so open it
        if (getBlockRow(blockNum) >= 0) {
            return;
        }
        blockDisplays[row].blockNum = blockNum;

        QMetaObject::invokeMethod(&kwp, "openBlock", Qt::QueuedConnection,
                                  Q_ARG(int, blockDisplays[row].blockNum));
    }
    else if (blockDisplays[row].blockNum != blockNum) {
        int oldNum = blockDisplays[row].blockNum;
        if (getBlockRow(blockNum) >= 0) {
            QMetaObject::invokeMethod(&kwp, "closeBlock", Qt::QueuedConnection,
                                      Q_ARG(int, oldNum));
            return;
        }
        blockDisplays[row].blockNum = blockNum;
        QMetaObject::invokeMethod(&kwp, "closeBlock", Qt::QueuedConnection,
                                  Q_ARG(int, oldNum));
        QMetaObject::invokeMethod(&kwp, "openBlock", Qt::QueuedConnection,
                                  Q_ARG(int, blockDisplays[row].blockNum));
    }
    else { // block is open, so close it
        QMetaObject::invokeMethod(&kwp, "closeBlock", Qt::QueuedConnection,
                                  Q_ARG(int, blockDisplays[row].blockNum));
    }
}

void MainWindow::spinChanged(int blockNum)
{
    blockNum++;
    for (int i = 0; i < 4; i++) {
        if (blockDisplays[i].spinBox == (QSpinBox*) sender()) {
            openCloseBlock(i);
        }
    }
}

void MainWindow::on_actionSerial_port_settings_triggered()
{
    serSettings->show();
}

void MainWindow::channelOpen(bool status)
{
    if (status) {
        ui->pushButton_openModule->setIcon(QIcon(":/resources/green.png"));
        ui->pushButton_openModule->setText("Close Module");
        ui->lineEdit_moduleNum->setEnabled(false);
        ui->comboBox_modules->setEnabled(false);
        ui->pushButton_refresh->setEnabled(false);
    }
    else {
        if (ui->pushButton_log->isChecked()) {
            startLogging(false);
            ui->pushButton_log->setChecked(false);
        }
        ui->pushButton_openModule->setIcon(QIcon(":/resources/red.png"));
        ui->pushButton_openModule->setText("Open Module");
        ui->lineEdit_moduleNum->setEnabled(true);
        ui->comboBox_modules->setEnabled(true);
        ui->pushButton_refresh->setEnabled(true);
    }
}

void MainWindow::blockOpen(int i)
{
    int row = getBlockRow(i);
    if (row < 0)
        return;
    blockDisplays[row].pushButton->setIcon(QIcon(":/resources/green.png"));
}

void MainWindow::blockClosed(int i)
{
    int row = getBlockRow(i);
    if (row < 0)
        return;
    blockDisplays[row].blockNum = -1;
    blockDisplays[row].pushButton->setIcon(QIcon(":/resources/red.png"));

    bool allBlocksClosed = true;
    for (int i = 0; i < 4; i++) {
        if (blockDisplays[row].blockNum >= 0) {
            allBlocksClosed = false;
            break;
        }
    }

    if (allBlocksClosed) {
        ui->lcdNumber->display(0);
    }
}

void MainWindow::on_actionReset_docks_triggered()
{
    ui->dockWidget_log->setFloating(false);
    this->removeDockWidget(ui->dockWidget_log);
    this->addDockWidget(Qt::RightDockWidgetArea, ui->dockWidget_log);
    ui->dockWidget_log->setVisible(true);

    ui->dockWidget_plot->setFloating(false);
    this->removeDockWidget(ui->dockWidget_plot);
    this->addDockWidget(Qt::BottomDockWidgetArea, ui->dockWidget_plot);
    ui->dockWidget_plot->setVisible(true);

    ui->dockWidget_module->setFloating(false);
    this->removeDockWidget(ui->dockWidget_module);
    this->addDockWidget(Qt::LeftDockWidgetArea, ui->dockWidget_module);
    ui->dockWidget_module->setVisible(true);

    ui->dockWidget_info->setFloating(false);
    this->removeDockWidget(ui->dockWidget_info);
    this->addDockWidget(Qt::RightDockWidgetArea, ui->dockWidget_info);
    ui->dockWidget_info->setVisible(true);
}

void MainWindow::startLogging(bool start)
{
    if (start) {
        if (kwp.getNumBlocksOpen() <= 0) {
            ui->pushButton_log->setChecked(false);
            return;
        }
        for (int i = 0; i < 4; i++) {
            blockDisplays[i].pushButton->setEnabled(false);
            blockDisplays[i].spinBox->setEnabled(false);
        }
        currentlyLogging = true;
        ui->pushButton_log->setIcon(QIcon(":/resources/green.png"));
        QMetaObject::invokeMethod(&kwp, "startLogging", Qt::QueuedConnection);
    }
    else {
        QMetaObject::invokeMethod(&kwp, "stopLogging", Qt::QueuedConnection);
        ui->pushButton_log->setIcon(QIcon(":/resources/red.png"));
        currentlyLogging = false;
        for (int i = 0; i < 4; i++) {
            blockDisplays[i].pushButton->setEnabled(true);
            blockDisplays[i].spinBox->setEnabled(true);
        }
    }
}

void MainWindow::labelsLoaded(bool ok)
{
    if (ok) {
        for (int i = 0; i < 4; i++) {
            blockLabels_t label = kwp.getBlockLabel(blockDisplays[i].spinBox->value());
            blockDisplays[i].blockTitle->setText(label.blockName);
            blockDisplays[i].spinBox->setToolTip(label.blockName);
            for (int j = 0; j < 4; j++) {
                blockDisplays[i].desc[j]->setText(label.desc[j]);
                blockDisplays[i].subDesc[j]->setText(label.subDesc[j]);
                blockDisplays[i].desc[j]->setToolTip(label.longDesc[j]);
                blockDisplays[i].subDesc[j]->setToolTip(label.longDesc[j]);
                blockDisplays[i].lineEdit[j]->setToolTip(label.longDesc[j]);
            }
        }

        QFileInfo tmp(kwp.getLabelFileName());
        QString text = "Label file: <a href=\"file:///" + tmp.absoluteFilePath() + "\">";
        text += tmp.fileName() + "</a>";

        ui->label_labelInfo->setText(text);
    }
    else {
        /*
        for (int i = 0; i < 4; i++) {
            blockDisplays[i].blockTitle->setText("");
            blockDisplays[i].spinBox->setToolTip("");
            for (int j = 0; j < 4; j++) {
                blockDisplays[i].desc[j]->setText("");
                blockDisplays[i].subDesc[j]->setText("");
                blockDisplays[i].desc[j]->setToolTip("");
                blockDisplays[i].subDesc[j]->setToolTip("");
                blockDisplays[i].lineEdit[j]->setToolTip("");
            }
        }
        */

        ui->label_labelInfo->setText("No labels loaded.");
    }
}

void MainWindow::moduleInfoReceived(QStringList info)
{
    ui->lineEdit_moduleInfo->setText(info.at(0));
    info.removeFirst();
    if (!info.empty()) {
        ui->lineEdit_extraInfo->setText(info.join(" "));
    }
}

void MainWindow::ecuInfoReceived(QStringList info)
{
    ui->lineEdit_ecuInfo->setText(info.join(" "));
}

void MainWindow::clearUI()
{
    ui->lineEdit_ecuInfo->clear();
    ui->lineEdit_moduleInfo->clear();
    ui->lineEdit_extraInfo->clear();

    for (int i = 0; i < 4; i++) {
        clearRow(i);
    }

    ui->label_labelInfo->setText("No labels loaded.");

    if (storedRow >= 0 && storedCol >= 0) {
        blockDisplays[storedRow].lineEdit[storedCol]->setStyleSheet("");
    }

    storedRow = -1;
    storedCol = -1;

    ui->label_selectedTitle->setText("Select a value");
    ui->label_selectedSub->clear();
    ui->label_selectedLong->clear();
    ui->label_selectedValue->clear();
    ui->label_selectedSub->hide();
    ui->label_selectedLong->hide();
    ui->label_selectedValue->hide();
}

void MainWindow::refreshModules(bool quickInit)
{
    if (quickInit) {
        ui->comboBox_modules->addItem("Engine", 1);
        ui->comboBox_modules->addItem("Transmission", 2);
        ui->comboBox_modules->addItem("Other");
        return;
    }

    QMap<int, moduleInfo_t> modules = kwp.getModuleList();
    QList<int> moduleNums = modules.keys();

    ui->comboBox_modules->clear();

    if (modules.empty()) {
        ui->comboBox_modules->addItem("Engine", 1);
        ui->comboBox_modules->addItem("Transmission", 2);
        ui->comboBox_modules->addItem("Other");
        return;
    }

    for (int i = 0; i < moduleNums.length(); i++) {
        int num = moduleNums.at(i);

        ui->comboBox_modules->addItem(modules[num].name, modules[num].number);

        switch (modules[num].status) {
        case 0x0:
            ui->comboBox_modules->setItemData(i, "Module OK", Qt::ToolTipRole);
            ui->comboBox_modules->setItemData(i, QBrush(Qt::darkGreen), Qt::ForegroundRole);
            break;
        case 0x2:
            ui->comboBox_modules->setItemData(i, "Malfunction", Qt::ToolTipRole);
            ui->comboBox_modules->setItemData(i, QBrush(Qt::darkRed), Qt::ForegroundRole);
            break;
        case 0x3:
            ui->comboBox_modules->setItemData(i, "Not registered", Qt::ToolTipRole);
            ui->comboBox_modules->setItemData(i, QBrush(Qt::red), Qt::ForegroundRole);
            break;
        case 0xC:
            ui->comboBox_modules->setItemData(i, "Can't be reached", Qt::ToolTipRole);
            ui->comboBox_modules->setItemData(i, QBrush(Qt::red), Qt::ForegroundRole);
            break;
        default:
            break;
        }
    }

    ui->comboBox_modules->addItem("Other");
}

void MainWindow::displayInfo(int i)
{
    int row = i / 4;
    int col = i % 4;

    int blockNum = blockDisplays[row].spinBox->value();

    // deselect already selected value
    if (storedRow == row && storedCol == col) {
        blockDisplays[row].lineEdit[col]->setStyleSheet("");
        storedRow = -1;
        storedCol = -1;

        ui->label_selectedTitle->setText("Select a value");
        ui->label_selectedSub->clear();
        ui->label_selectedLong->clear();
        ui->label_selectedValue->clear();
        ui->label_selectedUnits->clear();
        ui->label_selectedBinary->clear();
        ui->label_selectedSub->hide();
        ui->label_selectedLong->hide();
        ui->label_selectedValue->hide();
        ui->label_selectedUnits->hide();
        ui->label_selectedBinary->hide();
    }
    else { // select a different value
        if (storedRow >= 0 && storedCol >= 0) {
            blockDisplays[storedRow].lineEdit[storedCol]->setStyleSheet(""); // un-highlight old value
        }
        blockDisplays[row].lineEdit[col]->setStyleSheet("background-color:#FFFFCC");
        storedRow = row;
        storedCol = col;

        // hide irrelevant info if no labels are loaded
        if (ui->label_labelInfo->text() == "No labels loaded.") {
            ui->label_selectedTitle->setText("Block " + QString::number(blockNum)
                                             + " Value " + QString::number(col+1));
            ui->label_selectedSub->clear();
            ui->label_selectedLong->clear();
            ui->label_selectedBinary->clear();
            ui->label_selectedSub->hide();
            ui->label_selectedLong->hide();
            ui->label_selectedBinary->hide();
        }
        else {
            blockLabels_t label = kwp.getBlockLabel(blockNum);
            ui->label_selectedTitle->setText(label.desc[col]);
            ui->label_selectedSub->setText(label.subDesc[col]);
            ui->label_selectedLong->setText(label.longDesc[col]);
            ui->label_selectedBinary->setText(label.binDesc[col]);
            ui->label_selectedSub->show();
            ui->label_selectedLong->show();
            ui->label_selectedBinary->show();

            ui->label_selectedValue->clear(); // stops the delayed update of the values
            ui->label_selectedUnits->clear();
        }
        ui->label_selectedValue->show();
        ui->label_selectedUnits->show();
    }
}

void MainWindow::clearRow(int row, bool clearNum)
{
    blockDisplays[row].blockTitle->clear();
    blockDisplays[row].spinBox->setToolTip("");

    for (int j = 0; j < 4; j++) {
        blockDisplays[row].desc[j]->setText("Value " + QString::number(j+1));
        blockDisplays[row].subDesc[j]->clear();
        blockDisplays[row].lineEdit[j]->clear();
        blockDisplays[row].desc[j]->setToolTip("");
        blockDisplays[row].subDesc[j]->setToolTip("");
        blockDisplays[row].lineEdit[j]->setToolTip("");
        if (clearNum) {
            blockDisplays[row].spinBox->setValue(1);
        }
    }
}
