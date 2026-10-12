import QtQuick 2.0
import Sailfish.Silica 1.0
import harbour.sailtune 1.0
import "../components"

Page {
    allowedOrientations: Orientation.All

    RemorsePopup {
        id: remorse
    }

    SilicaListView {
        anchors.fill: parent
        header: PageHeader { title: qsTr("Albums") }

        PullDownMenu {
            // Only where every album is listed; the downloaded-only view
            // hides the albums this would add.
            visible: sources.active.canDownload && !sources.showingDownloadedOnly
                     && albums.downloadedCount < albums.count
            MenuItem {
                text: qsTr("Download all albums")
                onClicked: remorse.execute(qsTr("Downloading all albums"), function() { downloads.downloadAll() })
            }
        }

        model: LibraryModel {
            id: albums

            source: sources.active
            downloadedOnly: sources.showingDownloadedOnly
            revision: downloads.revision
            view: LibraryModel.Albums
        }

        delegate: TwoLineItem {
            image: model.image
            title: model.title
            description: model.artist
            downloaded: model.downloaded
            onClicked: pageStack.push(Qt.resolvedUrl("AlbumPage.qml"),
                                      { "albumId": model.id, "albumTitle": model.title,
                                        "albumArtist": model.artist, "albumImage": model.image })
        }

        VerticalScrollDecorator {}
    }
}
