TEMPLATE = app
TARGET = sailfish-waynergy-control
CONFIG += c++11

# Sailfish launches Silica apps through mapplauncherd/invoker.  It requires a
# position-independent executable; an ordinary ET_EXEC binary appears as an
# app that immediately fails to open from the launcher.
QMAKE_CXXFLAGS += -fPIE
# mapplauncherd loads the entry point from the executable with dlsym(), so
# main must be in the dynamic symbol table as well as the binary being PIE.
QMAKE_LFLAGS += -pie -Wl,--export-dynamic

QT += core gui qml quick

SOURCES += src/main.cpp \
           src/waynergycontroller.cpp

HEADERS += src/waynergycontroller.h

RESOURCES += qml.qrc

INSTALLS += target
target.path = /usr/bin
