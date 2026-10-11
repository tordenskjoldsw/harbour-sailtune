import QtQuick 2.0
import Sailfish.Silica 1.0
import "../components"

CoverBackground {
    Column {
        anchors.centerIn: parent
        width: parent.width - 2 * Theme.paddingLarge
        spacing: Theme.paddingMedium

        AlbumArt {
            visible: player.hasTrack
            width: parent.width
            source: player.image
        }

        Label {
            width: parent.width
            horizontalAlignment: Text.AlignHCenter
            maximumLineCount: 2
            wrapMode: Text.Wrap
            truncationMode: TruncationMode.Fade
            font.pixelSize: Theme.fontSizeSmall
            textFormat: Text.PlainText
            text: player.hasTrack ? player.title : (sources.active ? sources.active.name : "SailTune")
        }
    }

    CoverActionList {
        enabled: player.hasTrack

        CoverAction {
            iconSource: player.playing ? "image://theme/icon-cover-pause" : "image://theme/icon-cover-play"
            onTriggered: player.togglePlay()
        }
        CoverAction {
            iconSource: "image://theme/icon-cover-next-song"
            onTriggered: player.next()
        }
    }
}
