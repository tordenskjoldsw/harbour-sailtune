TARGET = harbour-musicspike

CONFIG += sailfishapp
QT += network

HEADERS += src/jellyfin.h

SOURCES += \
    src/main.cpp \
    src/jellyfin.cpp

QMAKE_LFLAGS += -Wl,-z,relro,-z,now -s

DISTFILES += \
    qml/harbour-musicspike.qml \
    qml/LoginPage.qml \
    qml/AlbumsPage.qml \
    qml/TracksPage.qml \
    qml/LogPage.qml \
    qml/OfflinePage.qml \
    rpm/harbour-musicspike.spec \
    harbour-musicspike.desktop
