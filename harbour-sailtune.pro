TARGET = harbour-sailtune

CONFIG += sailfishapp
QT += network

SOURCES += src/main.cpp

APP_VERSION = $$VERSION
isEmpty(APP_VERSION): APP_VERSION = 0.0.0+dev
DEFINES += APP_VERSION=\\\"$$APP_VERSION\\\"

SAILFISHAPP_ICONS = 86x86 108x108 128x128 172x172

QMAKE_LFLAGS += -Wl,-z,relro,-z,now -s

DISTFILES += \
    qml/harbour-sailtune.qml \
    qml/cover/CoverPage.qml \
    qml/pages/StartPage.qml \
    rpm/harbour-sailtune.changes \
    rpm/harbour-sailtune.spec \
    harbour-sailtune.desktop
