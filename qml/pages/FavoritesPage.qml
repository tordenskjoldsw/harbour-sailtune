import QtQuick 2.0
import Sailfish.Silica 1.0
import harbour.sailtune 1.0
import "../components"

// The favorite tracks of the active source.
Page {
    allowedOrientations: Orientation.All

    SilicaListView {
        anchors.fill: parent

        PullDownMenu {
            visible: sources.active.canDownload && tracks.count > 0
            MenuItem {
                text: qsTr("Remove download")
                visible: tracks.downloadedCount > 0
                onClicked: downloads.remove(tracks.rows())
            }
            MenuItem {
                text: qsTr("Download favorites")
                visible: tracks.downloadedCount < tracks.count
                onClicked: downloads.download(tracks.rows())
            }
        }

        header: Column {
            width: parent.width

            PageHeader { title: qsTr("Favorites") }

            DownloadState {
                tracks: tracks
            }
        }

        model: LibraryModel {
            id: tracks

            source: sources.active
            downloadedOnly: sources.showingDownloadedOnly
            view: LibraryModel.FavoriteTracks
            revision: downloads.revision
        }

        delegate: TwoLineItem {
            image: model.image
            title: model.title
            description: model.artist
            downloaded: model.downloaded
            menu: Component {
                TrackMenu {
                    trackId: model.id
                    favorite: model.favorite
                }
            }
            onClicked: player.playTracks(tracks.rows(), index)
        }

        ViewPlaceholder {
            enabled: tracks.count === 0
            text: qsTr("No favorites")
            hintText: qsTr("Long-press a track, or tap the heart while it plays.")
        }

        VerticalScrollDecorator {}
    }
}
