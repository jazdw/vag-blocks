#ifndef MAINWINDOW_H
#define MAINWINDOW_H

#include <QMainWindow>
#include <QPushButton>
#include <QSpinBox>
#include <QLabel>
#include <QLCDNumber>
#include <QVBoxLayout>
#include <QSignalMapper>
#include <QElapsedTimer>
#include <QSettings>

#include "kwp2000.h"
#include "clicklineedit.h"
#include "about.h"
#include "settings.h"

#include "qwt_plot.h"
#include "qwt_plot_curve.h"
#include "qwt_legend.h"
#include "qwt_legend_item.h"
#include "qwt_plot_grid.h"

const QString colorList[] = {
    "red", "green", "blue", "orange",
    "turquoise", "yellow", "fuchsia", "olive",
    "maroon", "lightpink", "greenyellow", "tan",
    "darkblue", "orangered", "grey", "teal"
};

typedef struct {
    QPushButton* pushButton;
    QSpinBox* spinBox;
    QLabel* blockTitle;
    QLabel* desc[4];
    QLabel* subDesc[4];
    clickLineEdit* lineEdit[4];
    int blockNum;
    double rate;
} blockWidgets;

typedef struct {
    QwtPlotCurve* curve;
    QVector<double>* data;
} curveAndData;

namespace Ui {
class MainWindow;
}

class MainWindow : public QMainWindow
{
    Q_OBJECT
    friend class settings;
public:
    explicit MainWindow(QWidget *parent = 0);
    ~MainWindow();
private:
    Ui::MainWindow *ui;
    serialSettingsDialog* serSettings;
    about* aboutDialog;
    settings* settingsDialog;

    kwp2000 kwp;
    blockWidgets blockDisplays[4];
    QSignalMapper mapButtons;
    QSignalMapper mapValueClick;
    QElapsedTimer perfTimer[4];
    QList<double> avgList;
    QSettings* appSettings;
    bool serialConfigured;
    bool showLogDock;
    bool showModuleDock;
    bool showInfoDock;
    bool showPlotDock;
    int logLevel;
    QStringList logBuffer;

    void setupBlockArray(QVBoxLayout *in);
    int getBlockRow(int blockNum);
    void clearRow(int row, bool clearNum = true);

    void closeEvent(QCloseEvent *event);
    void hideEvent(QHideEvent *event);
    void changeEvent(QEvent *event);
    void saveSettings();
    void restoreSettings();
    void saveDocks();
    void restoreDocks();
    void flushLogBuffer();

    int storedRow;
    int storedCol;

    bool currentlyLogging;

    void setupPlot();
    QTimer plotTimer;
    QVector<curveAndData> curves;
    QVector<double> timeAxis;

private slots:
    void plotUpdate();
    void log(const QString &txt, int logLevel = stdLog);
    void newBlockData(int blockNum);
    void connectToSerial();
    void openCloseBlock(int row);
    void spinChanged(int blockNum);
    void on_actionSerial_port_settings_triggered();
    void channelOpen(bool status);
    void blockOpen(int i);
    void blockClosed(int i);
    void on_actionReset_docks_triggered();
    void selectNewModule(int pos);
    void on_pushButton_openModule_clicked();
    void on_lineEdit_moduleNum_textChanged(const QString &arg1);
    void elmInitialised(bool ok);
    void portOpened(bool open);
    void portClosed();
    void startLogging(bool start);
    void labelsLoaded(bool ok);
    void moduleInfoReceived(QStringList info);
    void ecuInfoReceived(QStringList info);
    void clearUI();
    void refreshModules(bool quickInit = false);
    void displayInfo(int i);
    void sampleFormatChanged();
    void showCurve(QwtPlotItem *item, bool on);
    void loggingStarted();
    void updateSettings();
};

#endif // MAINWINDOW_H
