TARGET = harbour-sailtune

CONFIG += sailfishapp
QT += network sql dbus concurrent multimedia

HEADERS += \
    src/downloads/downloads.h \
    src/library/librarydatabase.h \
    src/library/librarymodel.h \
    src/player/player.h \
    src/sources/jellyfin.h \
    src/sources/jellyfinsource.h \
    src/sources/phonesource.h \
    src/sources/serversetup.h \
    src/sources/source.h \
    src/sources/sources.h

SOURCES += \
    src/main.cpp \
    src/downloads/downloads.cpp \
    src/library/librarydatabase.cpp \
    src/library/librarymodel.cpp \
    src/player/player.cpp \
    src/sources/jellyfin.cpp \
    src/sources/jellyfinsource.cpp \
    src/sources/phonesource.cpp \
    src/sources/serversetup.cpp \
    src/sources/source.cpp \
    src/sources/sources.cpp

APP_VERSION = $$VERSION
isEmpty(APP_VERSION): APP_VERSION = 0.0.0+dev
DEFINES += APP_VERSION=\\\"$$APP_VERSION\\\"

SAILFISHAPP_ICONS = 86x86 108x108 128x128 172x172

QMAKE_LFLAGS += -Wl,-z,relro,-z,now -s

DISTFILES += \
    qml/harbour-sailtune.qml \
    qml/components/AlbumArt.qml \
    qml/components/DownloadState.qml \
    qml/components/MiniPlayer.qml \
    qml/components/Plain.js \
    qml/components/TrackMenu.qml \
    qml/components/TwoLineItem.qml \
    qml/cover/CoverPage.qml \
    qml/pages/AlbumPage.qml \
    qml/pages/AlbumsPage.qml \
    qml/pages/ArtistPage.qml \
    qml/pages/ArtistsPage.qml \
    qml/pages/FavoritesPage.qml \
    qml/pages/LibraryPage.qml \
    qml/pages/NowPlayingPage.qml \
    qml/pages/PlaylistPage.qml \
    qml/pages/PlaylistPickerPage.qml \
    qml/pages/PlaylistsPage.qml \
    qml/pages/SearchPage.qml \
    qml/pages/ServerPage.qml \
    qml/pages/SettingsPage.qml \
    qml/pages/SourcesPage.qml \
    rpm/harbour-sailtune.changes \
    rpm/harbour-sailtune.spec \
    harbour-sailtune.desktop
