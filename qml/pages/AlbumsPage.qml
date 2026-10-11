import QtQuick 2.0
import Sailfish.Silica 1.0
import harbour.sailtune 1.0
import "../components"

Page {
    allowedOrientations: Orientation.All

    SilicaListView {
        anchors.fill: parent
        header: PageHeader { title: qsTr("Albums") }

        model: LibraryModel {
            source: sources.active
            downloadedOnly: sources.showingDownloadedOnly
            revision: downloads.revision
            view: LibraryModel.Albums
        }

        delegate: TwoLineItem {
            image: model.image
            title: model.title
            description: model.artist
            onClicked: pageStack.push(Qt.resolvedUrl("AlbumPage.qml"),
                                      { "albumId": model.id, "albumTitle": model.title,
                                        "albumArtist": model.artist, "albumImage": model.image })
        }

        VerticalScrollDecorator {}
    }
}
