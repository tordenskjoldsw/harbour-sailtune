import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: page

    allowedOrientations: Orientation.All

    Component.onCompleted: jellyfin.loadAlbums("")

    Connections {
        target: jellyfin
        // The signal's argument is named albums.
        onAlbumsLoaded: {
            albumModel.clear()
            for (var i = 0; i < albums.length; ++i)
                albumModel.append(albums[i])
        }
    }

    ListModel { id: albumModel }

    SilicaListView {
        anchors.fill: parent
        model: albumModel

        PullDownMenu {
            MenuItem {
                text: "Log"
                onClicked: pageStack.push(Qt.resolvedUrl("LogPage.qml"))
            }
            MenuItem {
                text: "Downloaded music"
                onClicked: pageStack.push(Qt.resolvedUrl("OfflinePage.qml"))
            }
        }

        header: Column {
            width: parent.width

            PageHeader { title: "Albums" }

            SearchField {
                width: parent.width
                placeholderText: "Search albums"
                EnterKey.onClicked: jellyfin.loadAlbums(text)
            }

            BusyIndicator {
                anchors.horizontalCenter: parent.horizontalCenter
                running: jellyfin.busy
                visible: running
            }
        }

        delegate: ListItem {
            onClicked: pageStack.push(Qt.resolvedUrl("TracksPage.qml"),
                                      { "albumId": model.id, "albumName": model.name })

            Column {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                anchors.verticalCenter: parent.verticalCenter

                Label {
                    width: parent.width
                    truncationMode: TruncationMode.Fade
                    text: model.name
                }
                Label {
                    width: parent.width
                    truncationMode: TruncationMode.Fade
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: Theme.secondaryColor
                    text: model.artist + (model.year > 0 ? " (" + model.year + ")" : "")
                }
            }
        }

        VerticalScrollDecorator {}
    }
}
