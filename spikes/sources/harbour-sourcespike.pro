TARGET = harbour-sourcespike

CONFIG += sailfishapp
QT += network dbus concurrent

HEADERS += src/probe.h

SOURCES += \
    src/main.cpp \
    src/probe.cpp

QMAKE_LFLAGS += -Wl,-z,relro,-z,now -s

DISTFILES += \
    qml/harbour-sourcespike.qml \
    rpm/harbour-sourcespike.spec \
    harbour-sourcespike.desktop
