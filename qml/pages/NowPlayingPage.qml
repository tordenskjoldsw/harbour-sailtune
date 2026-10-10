import QtQuick 2.0
import Sailfish.Silica 1.0
import "../components"

// The current track with its cover and every control.
Page {
    function time(milliseconds) {
        var seconds = Math.floor(milliseconds / 1000)
        var rest = seconds % 60
        return Math.floor(seconds / 60) + ":" + (rest < 10 ? "0" : "") + rest
    }

    allowedOrientations: Orientation.All

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column

            width: parent.width
            spacing: Theme.paddingLarge

            PageHeader {
                title: player.queueLength > 0
                       ? qsTr("Track %1 of %2").arg(player.queueIndex + 1).arg(player.queueLength) : ""
            }

            AlbumArt {
                anchors.horizontalCenter: parent.horizontalCenter
                width: Math.min(parent.width - 2 * Theme.horizontalPageMargin, Screen.height / 2)
                source: player.image
            }

            Column {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin

                Label {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    font.pixelSize: Theme.fontSizeLarge
                    color: Theme.highlightColor
                    text: player.title
                }
                Label {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    color: Theme.secondaryHighlightColor
                    text: player.artist + (player.album.length > 0 ? " - " + player.album : "")
                }
                Label {
                    width: parent.width
                    horizontalAlignment: Text.AlignHCenter
                    wrapMode: Text.Wrap
                    color: Theme.errorColor
                    visible: text.length > 0
                    text: player.problem
                }
            }

            Slider {
                id: slider

                width: parent.width
                minimumValue: 0
                maximumValue: Math.max(1, player.duration)
                enabled: player.seekable
                valueText: time(value)
                label: time(player.duration)
                onReleased: player.seek(value)

                // Follows the music unless the user drags it.
                Connections {
                    target: player
                    onPositionChanged: {
                        if (!slider.down)
                            slider.value = player.position
                    }
                }
                Component.onCompleted: value = player.position
            }

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: Theme.paddingMedium

                IconButton {
                    icon.source: "image://theme/icon-m-previous"
                    onClicked: player.previous()
                }
                IconButton {
                    icon.source: "image://theme/icon-m-10s-back"
                    enabled: player.seekable
                    onClicked: player.seekBy(-10000)
                }
                IconButton {
                    icon.source: player.playing ? "image://theme/icon-l-pause" : "image://theme/icon-l-play"
                    onClicked: player.togglePlay()
                }
                IconButton {
                    icon.source: "image://theme/icon-m-10s-forward"
                    enabled: player.seekable
                    onClicked: player.seekBy(10000)
                }
                IconButton {
                    icon.source: "image://theme/icon-m-next"
                    enabled: player.queueIndex + 1 < player.queueLength
                    onClicked: player.next()
                }
            }
        }
    }
}
