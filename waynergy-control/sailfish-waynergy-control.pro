TEMPLATE = app
TARGET = sailfish-waynergy-control
CONFIG += c++11

QT += core gui qml quick

SOURCES += src/main.cpp \
           src/waynergycontroller.cpp

HEADERS += src/waynergycontroller.h

RESOURCES += qml.qrc

INSTALLS += target
target.path = /usr/bin
