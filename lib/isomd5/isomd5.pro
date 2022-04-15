TEMPLATE = lib

CONFIG += staticlib

QT += core

DESTDIR = ../

HEADERS += libcheckisomd5.h

SOURCES += libcheckisomd5.cpp

QMAKE_MACOSX_DEPLOYMENT_TARGET = 10.9
