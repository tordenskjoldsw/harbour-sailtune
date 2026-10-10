import QtQuick 2.0
import QtMultimedia 5.6
import Sailfish.Silica 1.0

ApplicationWindow {
    id: app

    Audio {
        id: player

        audioRole: Audio.MusicRole
    }

    initialPage: Component {
        Page {
            allowedOrientations: Orientation.All

            SilicaListView {
                anchors.fill: parent
                model: probe.songs

                PullDownMenu {
                    MenuItem {
                        text: "Find Jellyfin servers"
                        onClicked: probe.discoverServers()
                    }
                    MenuItem {
                        text: "Count album art"
                        onClicked: probe.countMediaArt()
                    }
                    MenuItem {
                        text: "Read music index (Turtle)"
                        onClicked: probe.queryIndex(0)
                    }
                    MenuItem {
                        text: "Read music index (JSON-LD)"
                        onClicked: probe.queryIndex(2)
                    }
                }

                header: Column {
                    width: parent.width

                    PageHeader { title: "Source spike" }

                    Label {
                        x: Theme.horizontalPageMargin
                        width: parent.width - 2 * Theme.horizontalPageMargin
                        wrapMode: Text.WrapAnywhere
                        font.pixelSize: Theme.fontSizeExtraSmall
                        color: Theme.highlightColor
                        text: probe.report
                              + (player.playbackState === Audio.PlayingState ? "\nplaying a local file" : "")
                              + (player.errorString.length > 0 ? "\nplayer: " + player.errorString : "")
                    }

                    SectionHeader { text: "Songs (" + probe.songs.length + ")" }
                }

                delegate: ListItem {
                    onClicked: {
                        player.source = modelData.url
                        player.play()
                    }

                    Column {
                        x: Theme.horizontalPageMargin
                        width: parent.width - 2 * Theme.horizontalPageMargin
                        anchors.verticalCenter: parent.verticalCenter

                        Label {
                            width: parent.width
                            truncationMode: TruncationMode.Fade
                            text: (modelData.track ? modelData.track + ". " : "") + modelData.title
                        }
                        Label {
                            width: parent.width
                            truncationMode: TruncationMode.Fade
                            font.pixelSize: Theme.fontSizeExtraSmall
                            color: Theme.secondaryColor
                            text: modelData.artist + " - " + modelData.album
                                  + (modelData.seconds ? " (" + modelData.seconds + " s)" : "")
                        }
                    }
                }

                VerticalScrollDecorator {}
            }
        }
    }
}
