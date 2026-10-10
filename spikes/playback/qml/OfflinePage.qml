import QtQuick 2.0
import Sailfish.Silica 1.0

// The downloaded tracks by album, read from the app's storage without the
// server; a tap plays the album from that track.
Page {
    id: page

    property var tracks: []

    function load() {
        var list = jellyfin.downloads()
        list.sort(function(a, b) {
            if (a.album !== b.album)
                return a.album < b.album ? -1 : 1
            return a.number - b.number
        })
        tracks = list
        trackModel.clear()
        for (var i = 0; i < list.length; ++i)
            trackModel.append(list[i])
        jellyfin.log("offline list: " + list.length + " tracks")
    }

    function albumTracks(index) {
        var album = tracks[index].album
        var result = []
        var start = 0
        for (var i = 0; i < tracks.length; ++i) {
            if (tracks[i].album !== album)
                continue
            if (i === index)
                start = result.length
            result.push(tracks[i])
        }
        return { "tracks": result, "start": start }
    }

    allowedOrientations: Orientation.All

    Component.onCompleted: load()

    ListModel { id: trackModel }

    SilicaListView {
        anchors.fill: parent
        model: trackModel

        PullDownMenu {
            MenuItem {
                text: "Log"
                onClicked: pageStack.push(Qt.resolvedUrl("LogPage.qml"))
            }
        }

        header: PageHeader {
            title: "Downloaded music"
            description: trackModel.count + " tracks"
        }

        section {
            property: "album"
            delegate: SectionHeader { text: section }
        }

        delegate: ListItem {
            onClicked: {
                var album = page.albumTracks(index)
                app.playQueue(album.tracks, album.start, true)
            }

            Column {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                anchors.verticalCenter: parent.verticalCenter

                Label {
                    width: parent.width
                    truncationMode: TruncationMode.Fade
                    text: (model.number > 0 ? model.number + ". " : "") + model.name
                }
                Label {
                    width: parent.width
                    truncationMode: TruncationMode.Fade
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: Theme.secondaryColor
                    text: model.artist + (model.artist.length > 0 ? ", " : "") + model.codec
                }
            }
        }

        ViewPlaceholder {
            enabled: trackModel.count === 0
            text: "No downloads yet"
        }

        VerticalScrollDecorator {}
    }
}
