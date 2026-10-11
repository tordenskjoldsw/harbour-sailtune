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
    // Disc headers only for albums of more than one known disc; 0 means
    // the file names none.
    property bool severalDiscs
    // Per row: whether a new disc starts there.
    property var discStarts: []

    function duration(seconds) {
        var rest = seconds % 60
        return Math.floor(seconds / 60) + ":" + (rest < 10 ? "0" : "") + rest
    }

    allowedOrientations: Orientation.All

    SilicaListView {
        anchors.fill: parent

        PullDownMenu {
            visible: sources.active.canDownload || sources.active.canEditPlaylists
            MenuItem {
                text: qsTr("Add album to playlist")
                visible: sources.active.canEditPlaylists && tracks.count > 0
                enabled: sources.active.reachable && sources.active.needsLogin !== true
                onClicked: {
                    var rows = tracks.rows()
                    var ids = []
                    for (var i = 0; i < rows.length; ++i)
                        ids.push(rows[i].id)
                    pageStack.push(Qt.resolvedUrl("PlaylistPickerPage.qml"), { "trackIds": ids })
                }
            }
            MenuItem {
                text: qsTr("Remove download")
                visible: sources.active.canDownload && tracks.downloadedCount > 0
                onClicked: downloads.remove(tracks.rows())
            }
            MenuItem {
                text: qsTr("Download album")
                visible: sources.active.canDownload && tracks.downloadedCount < tracks.count
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
            downloadedOnly: sources.showingDownloadedOnly
            view: LibraryModel.AlbumTracks
            filter: albumId
            revision: downloads.revision
            onCountChanged: {
                var rows = this.rows()
                var discs = {}
                var starts = []
                for (var i = 0; i < rows.length; ++i) {
                    if (rows[i].disc > 0)
                        discs[rows[i].disc] = true
                    starts.push(rows[i].disc > 0 && (i === 0 || rows[i - 1].disc !== rows[i].disc))
                }
                severalDiscs = Object.keys(discs).length > 1
                discStarts = starts
            }
        }

        // The header is part of the row: a ListView section header that
        // changed its height after the layout left a gap on the device
        // (screenshot, 2026-10-11).
        delegate: Column {
            width: ListView.view.width

            SectionHeader {
                visible: severalDiscs && discStarts[index] === true
                text: qsTr("Disc %1").arg(model.disc)
            }

            TwoLineItem {
                width: parent.width
                showImage: false
                downloaded: model.downloaded
                favorite: model.favorite
                menu: Component {
                    TrackMenu {
                        trackId: model.id
                        favorite: model.favorite
                    }
                }
                highlighted: down || player.trackId === model.id
                onClicked: player.playTracks(tracks.rows(), index)
                title: (model.number > 0 ? model.number + ". " : "") + model.title
                description: (model.artist !== albumArtist ? model.artist + ", " : "")
                             + (model.seconds > 0 ? duration(model.seconds) : "")
            }
        }

        VerticalScrollDecorator {}
    }
}
