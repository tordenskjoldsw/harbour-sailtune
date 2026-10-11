import QtQuick 2.0
import Sailfish.Silica 1.0
import harbour.sailtune 1.0
import "../components"

Page {
    property string playlistId
    property string playlistName

    allowedOrientations: Orientation.All

    SilicaListView {
        anchors.fill: parent

        PullDownMenu {
            visible: sources.active.canDownload
            MenuItem {
                text: qsTr("Remove download")
                visible: tracks.downloadedCount > 0
                onClicked: downloads.remove(tracks.rows())
            }
            MenuItem {
                text: qsTr("Download playlist")
                visible: tracks.downloadedCount < tracks.count
                onClicked: downloads.download(tracks.rows())
            }
        }

        header: Column {
            width: parent.width

            PageHeader { title: playlistName }

            DownloadState {
                tracks: tracks
            }
        }

        model: LibraryModel {
            id: tracks

            source: sources.active
            downloadedOnly: sources.showingDownloadedOnly
            view: LibraryModel.PlaylistTracks
            filter: playlistId
            revision: downloads.revision
        }

        delegate: TwoLineItem {
            image: model.image
            title: model.title
            description: model.artist
            downloaded: model.downloaded
            onClicked: player.playTracks(tracks.rows(), index)
        }

        VerticalScrollDecorator {}
    }
}
