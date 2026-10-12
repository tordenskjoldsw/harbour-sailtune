import QtQuick 2.0
import Sailfish.Silica 1.0
import "../components/Plain.js" as Plain
import harbour.sailtune 1.0
import "../components"

// An artist's albums.
Page {
    property string artistId
    property string artistName

    allowedOrientations: Orientation.All

    SilicaListView {
        anchors.fill: parent
        header: PageHeader { title: Plain.text(artistName) }

        model: LibraryModel {
            source: sources.active
            downloadedOnly: sources.showingDownloadedOnly
            revision: downloads.revision
            view: LibraryModel.ArtistAlbums
            filter: artistId
        }

        delegate: TwoLineItem {
            image: model.image
            title: model.title
            description: model.year > 0 ? model.year : ""
            downloaded: model.downloaded
            onClicked: pageStack.push(Qt.resolvedUrl("AlbumPage.qml"),
                                      { "albumId": model.id, "albumTitle": model.title,
                                        "albumArtist": model.artist, "albumImage": model.image })
        }

        VerticalScrollDecorator {}
    }
}
