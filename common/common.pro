TEMPLATE = lib
TARGET = xmlcommon
CONFIG += staticlib
SOURCES = \
   fileprovider.cpp \
   messagehandler.cpp \
   nsmanager.cpp \
   parsercontext.cpp \
   qname.cpp

HEADERS = fileprovider.h

QT -= gui
QT += xml network

INCLUDEPATH += $${PWD}/.. $${PWD}/../libkode

include ($${PWD}/../variables.pri)
DEFINES -= QT_NO_CAST_TO_ASCII QBA_NO_CAST_TO_VOID QBA_NO_CAST_TO_ASCII QT_NO_CAST_FROM_ASCII

