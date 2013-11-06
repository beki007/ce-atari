#-------------------------------------------------
#
# Project created by QtCreator 2013-07-04T13:07:02
#
#-------------------------------------------------

QT       += core gui

TARGET = acsiSwTest
TEMPLATE = app


SOURCES +=  main.cpp \
            mainwindow.cpp \
            cconusb.cpp \
            ccorethread.cpp \
            acsidatatrans.cpp \
            settings.cpp \
            native/scsi.cpp \
            native/datamedia.cpp \
            native/nomedia.cpp \
            native/testmedia.cpp \
            config/configcomponent.cpp \
            config/configscreen_main.cpp \
            config/configstream.cpp \
            translated/translateddisk.cpp

HEADERS  += mainwindow.h \
            cconusb.h \
            ccorethread.h \
            ftd2xx.h \
            global.h \
            datatypes.h \
            sleeper.h \
            settings.h \
            native/scsi.h \
            native/scsi_defs.h \
            acsidatatrans.h \
            native/datamedia.h \
            native/nomedia.h \
            native/imedia.h \
            native/testmedia.h \
            config/configcomponent.h \
            config/configscreen_main.h \
            config/configstream.h \
            config/keys.h \
            translated/translateddisk.h \
            translated/gemdos.h \
            translated/gemdos_errno.h

FORMS    += mainwindow.ui
