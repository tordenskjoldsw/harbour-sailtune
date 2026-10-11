import QtQuick 2.0
import Sailfish.Silica 1.0
import harbour.sailtune 1.0
import "../components"

// An album's tracks under its cover.
Page {
    property string albumId
    property string albumTitle
    property string albumArtist
    property url albumImage
    // Disc headers only for albums of more than one disc.
    property bool severalDiscs

    function duration(seconds) {
        var rest = seconds % 60
        return Math.floor(seconds / 60) + ":" + (rest < 10 ? "0" : "") + rest
    }

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
                text: qsTr("Download album")
                visible: tracks.downloadedCount < tracks.count
                onClicked: downloads.download(tracks.rows())
            }
        }

        header: Column {
            width: parent.width
            spacing: Theme.paddingMedium

            PageHeader {
                title: albumTitle
                description: albumArtist
            }

            DownloadState {
                tracks: tracks
            }

            AlbumArt {
                anchors.horizontalCenter: parent.horizontalCenter
                width: Math.min(parent.width, Screen.height) / 2
                source: albumImage
            }

            Item { width: 1; height: Theme.paddingMedium }
        }

        model: LibraryModel {
            id: tracks

            source: sources.active
            view: LibraryModel.AlbumTracks
            filter: albumId
            revision: downloads.revision
            onCountChanged: {
                var rows = this.rows()
                var discs = {}
                for (var i = 0; i < rows.length; ++i)
                    discs[rows[i].disc] = true
                severalDiscs = Object.keys(discs).length > 1
            }
        }

        section {
            property: "disc"
            delegate: SectionHeader {
                visible: severalDiscs
                height: visible ? implicitHeight : 0
                text: qsTr("Disc %1").arg(section)
            }
        }

        delegate: TwoLineItem {
            showImage: false
            downloaded: model.downloaded
            highlighted: down || player.trackId === model.id
            onClicked: player.playTracks(tracks.rows(), index)
            title: (model.number > 0 ? model.number + ". " : "") + model.title
            description: (model.artist !== albumArtist ? model.artist + ", " : "")
                         + (model.seconds > 0 ? duration(model.seconds) : "")
        }

        VerticalScrollDecorator {}
    }
}
