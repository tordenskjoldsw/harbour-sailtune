import QtQuick 2.0
import Sailfish.Silica 1.0

// The current track at the bottom of every page; a tap opens the player.
DockedPanel {
    width: parent.width
    height: Theme.itemSizeLarge
    dock: Dock.Bottom
    open: player.hasTrack

    BackgroundItem {
        anchors.fill: parent
        onClicked: pageStack.push(Qt.resolvedUrl("../pages/NowPlayingPage.qml"))

        AlbumArt {
            id: art

            anchors.verticalCenter: parent.verticalCenter
            x: Theme.horizontalPageMargin
            width: parent.height - 2 * Theme.paddingMedium
            source: player.image
        }

        Column {
            anchors {
                verticalCenter: parent.verticalCenter
                left: art.right
                leftMargin: Theme.paddingMedium
                right: buttons.left
                rightMargin: Theme.paddingSmall
            }

            Label {
                width: parent.width
                truncationMode: TruncationMode.Fade
                text: player.title
            }
            Label {
                width: parent.width
                truncationMode: TruncationMode.Fade
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
                text: player.problem.length > 0 ? player.problem : player.artist
            }
        }

        Row {
            id: buttons

            anchors {
                verticalCenter: parent.verticalCenter
                right: parent.right
                rightMargin: Theme.paddingMedium
            }

            IconButton {
                icon.source: "image://theme/icon-m-10s-back"
                enabled: player.seekable
                onClicked: player.seekBy(-10000)
            }
            IconButton {
                icon.source: player.playing ? "image://theme/icon-m-pause" : "image://theme/icon-m-play"
                onClicked: player.togglePlay()
            }
            IconButton {
                icon.source: "image://theme/icon-m-10s-forward"
                enabled: player.seekable
                onClicked: player.seekBy(10000)
            }
        }
    }

    // How far the track has played, as a thin line along the top.
    Rectangle {
        anchors.top: parent.top
        height: Theme.paddingSmall / 2
        width: player.duration > 0 ? parent.width * player.position / player.duration : 0
        color: Theme.highlightColor
    }
}
