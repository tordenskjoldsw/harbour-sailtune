import QtQuick 2.0
import Sailfish.Silica 1.0

// An album's tracks with what the server stores (codec, sample rate, bit
// depth, bitrate), to stream, to download or to play from the downloads.
Page {
    id: page

    property string albumId
    property string albumName
    property var trackList: []
    property int downloading: -1
    property int downloadedCount

    function quality(track) {
        var parts = [ track.codec ]
        if (track.sampleRate > 0)
            parts.push((track.sampleRate / 1000) + " kHz" + (track.bitDepth > 0 ? "/" + track.bitDepth + " bit" : ""))
        if (track.kbps > 0)
            parts.push(track.kbps + " kbps")
        parts.push((track.bytes / 1048576).toFixed(1) + " MB")
        return parts.join(", ")
    }

    function refreshDownloaded() {
        var count = 0
        for (var i = 0; i < trackModel.count; ++i) {
            var track = trackModel.get(i)
            var local = jellyfin.localFile(track.id, track.container).toString().length > 0
            trackModel.setProperty(i, "local", local)
            if (local)
                ++count
        }
        downloadedCount = count
    }

    // One download at a time, so the log shows each file's speed.
    function downloadNext() {
        for (var i = 0; i < trackModel.count; ++i) {
            if (!trackModel.get(i).local) {
                downloading = i
                jellyfin.download(page.trackList[i])
                return
            }
        }
        downloading = -1
    }

    allowedOrientations: Orientation.All

    Component.onCompleted: jellyfin.loadTracks(albumId)

    Connections {
        target: jellyfin
        // The signal's argument is named tracks.
        onTracksLoaded: {
            page.trackList = tracks
            trackModel.clear()
            for (var i = 0; i < tracks.length; ++i) {
                var track = tracks[i]
                track.local = false
                trackModel.append(track)
            }
            page.refreshDownloaded()
        }
        onDownloadFinished: {
            page.refreshDownloaded()
            if (page.downloading >= 0 && ok)
                page.downloadNext()
            else
                page.downloading = -1
        }
    }

    ListModel { id: trackModel }

    SilicaListView {
        anchors.fill: parent
        model: trackModel

        PullDownMenu {
            MenuItem {
                text: "Log"
                onClicked: pageStack.push(Qt.resolvedUrl("LogPage.qml"))
            }
            MenuItem {
                text: "Download album"
                enabled: page.downloading < 0 && page.downloadedCount < trackModel.count
                onClicked: page.downloadNext()
            }
            MenuItem {
                text: "Play album from downloads"
                enabled: trackModel.count > 0 && page.downloadedCount === trackModel.count
                onClicked: app.playQueue(page.trackList, 0, true)
            }
            MenuItem {
                text: "Stream album"
                enabled: trackModel.count > 0
                onClicked: app.playQueue(page.trackList, 0, false)
            }
        }

        header: Column {
            width: parent.width

            PageHeader {
                title: page.albumName
                description: page.downloading >= 0
                             ? "Downloading " + (page.downloading + 1) + " of " + trackModel.count
                             : "Downloaded " + page.downloadedCount + " of " + trackModel.count
            }
        }

        delegate: ListItem {
            contentHeight: Theme.itemSizeMedium
            onClicked: app.playQueue(page.trackList, index, false)
            menu: ContextMenu {
                MenuItem {
                    text: "Stream from here"
                    onClicked: app.playQueue(page.trackList, index, false)
                }
                MenuItem {
                    text: "Play the download"
                    visible: model.local
                    onClicked: app.playQueue([ page.trackList[index] ], 0, true)
                }
            }

            Column {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                anchors.verticalCenter: parent.verticalCenter

                Label {
                    width: parent.width
                    truncationMode: TruncationMode.Fade
                    color: model.local ? Theme.highlightColor : Theme.primaryColor
                    text: model.number + ". " + model.name
                }
                Label {
                    width: parent.width
                    truncationMode: TruncationMode.Fade
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: Theme.secondaryColor
                    text: page.quality(model) + (model.local ? ", downloaded" : "")
                }
            }
        }

        VerticalScrollDecorator {}
    }
}
