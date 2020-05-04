TEMPLATE = lib
TARGET = xmlschema
CONFIG += staticlib

include(schema.pri)

QT -= gui
QT += xml

INCLUDEPATH += $${PWD}/.. $${PWD}/../code_generation

include($${PWD}/../variables.pri)
DEFINES -= QT_NO_CAST_TO_ASCII QBA_NO_CAST_TO_VOID QBA_NO_CAST_TO_ASCII QT_NO_CAST_FROM_ASCII
