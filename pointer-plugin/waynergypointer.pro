TEMPLATE = lib
CONFIG += plugin c++11
QT += core qml
TARGET = waynergypointer
SOURCES += pointerfeed.cpp pointercalibration.cpp plugin.cpp
HEADERS += pointerfeed.h pointercalibration.h

INCLUDEPATH += /home/nemo/cursor-build/sysroot/usr/include/qt5 \
               /home/nemo/cursor-build/sysroot/usr/include/qt5/QtCore \
               /home/nemo/cursor-build/sysroot/usr/include/qt5/QtQml \
               /home/nemo/cursor-build/sysroot/usr/include/qt5/QtGui

LIBS += -L/home/nemo/cursor-build/sysroot/usr/lib
