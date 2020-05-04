TEMPLATE = lib
CONFIG += staticlib
TARGET = kode

include(code_generation.pri)

QT -= gui

INCLUDEPATH += $${PWD}

include($${PWD}/../variables.pri)
DEFINES -= QT_NO_CAST_TO_ASCII QBA_NO_CAST_TO_VOID QBA_NO_CAST_TO_ASCII QT_NO_CAST_FROM_ASCII



