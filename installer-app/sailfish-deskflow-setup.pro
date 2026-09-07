TEMPLATE = app
TARGET = sailfish-deskflow-setup
CONFIG += c++11

QT += core gui qml quick

SOURCES += src/main.cpp \
           src/installercontroller.cpp

HEADERS += src/installercontroller.h

RESOURCES += qml.qrc

LIBS += -lutil

INSTALLS += target
target.path = /usr/bin
