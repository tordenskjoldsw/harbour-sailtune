TARGET = harbour-sailtune

CONFIG += sailfishapp

SOURCES += src/main.cpp

SAILFISHAPP_ICONS = 86x86 108x108 128x128 172x172

DISTFILES += \
    qml/harbour-sailtune.qml \
    rpm/harbour-sailtune.spec \
    harbour-sailtune.desktop
QMAKE_LFLAGS += -Wl,-z,relro,-z,now -s
