#-------------------------------------------------
#
# Project created by QtCreator 2012-10-13T14:44:42
#
#-------------------------------------------------

QT       += core gui

TARGET = vagblocks
TEMPLATE = app

VERSION = 1.0.0
SVN_REV = $$system(svnversion)
VERSION_STR = '\\"$${VERSION}\\"'
SVN_REV_STR = '\\"$${SVN_REV}\\"'
DEFINES += APP_VERSION=\"$${VERSION_STR}\"
DEFINES += APP_SVN_REV=\"$${SVN_REV_STR}\"

SOURCES += main.cpp\
        mainwindow.cpp \
    elm327.cpp \
    tp20.cpp \
    canframe.cpp \
    kwp2000.cpp \
    util.cpp \
    serialsettings.cpp \
    monitor.cpp \
    clicklineedit.cpp \
    about.cpp \
    settings.cpp

HEADERS  += mainwindow.h \
    elm327.h \
    tp20.h \
    canframe.h \
    kwp2000.h \
    util.h \
    serialsettings.h \
    monitor.h \
    clicklineedit.h \
    about.h \
    settings.h

FORMS    += mainwindow.ui \
    serialsettings.ui \
    about.ui \
    settings.ui

# used to disable "imp" macro in library function names for qtserialport
static {
    DEFINES += STATIC_BUILD
}

!win32 {
    INCLUDEPATH += "../qtserialport/src" "../qwt-6.0/src"
    LIBS += -L../qtserialport/src -L../qwt-6.0/lib
    LIBS += -lSerialPort -lqwt
}

win32 {
    RC_FILE = resources/winres.rc
    INCLUDEPATH += "../qtserialport/src" "C:/qwt-6.0/src"
    LIBS += -LC:/qwt-6.0/lib
    LIBS += -lSerialPort

    CONFIG(release, debug|release) {
        LIBS += -L../qtserialport/src/release -lqwt
        static {
            # needed for qtserialport
            LIBS += -lsetupapi -ladvapi32
        }
    }
    else {
        LIBS += -L../qtserialport/src/debug -lqwtd
    }
}

RESOURCES += \
    icons.qrc

OTHER_FILES += \
    resources/winres.rc
