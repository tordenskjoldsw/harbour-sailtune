import QtQuick 2.0
import Sailfish.Silica 1.0
import harbour.sailtune 1.0

// The space the downloads use, the largest downloaded albums of the
// active source, the state of the download queue, and the version.
Page {
    id: page

    property var storage: downloads.storage()
    property var albums: downloads.largestAlbums(10)

    function reload() {
        storage = downloads.storage()
        albums = downloads.largestAlbums(10)
    }

    allowedOrientations: Orientation.All

    Connections {
        target: downloads
        onRevisionChanged: page.reload()
    }

    RemorsePopup {
        id: remorse
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column

            width: parent.width

            PageHeader { title: qsTr("Settings") }

            SectionHeader { text: qsTr("Storage") }

            DetailItem {
                label: qsTr("Downloads")
                value: Format.formatFileSize(page.storage.used)
            }
            DetailItem {
                label: qsTr("Free on the phone")
                value: Format.formatFileSize(page.storage.free)
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryHighlightColor
                text: qsTr("Downloads pause before less than 2 GB stay free on the phone.")
            }

            SectionHeader {
                visible: page.albums.length > 0
                text: qsTr("Largest downloaded albums")
            }

            Repeater {
                model: page.albums

                ListItem {
                    id: albumItem

                    width: column.width
                    contentHeight: Theme.itemSizeMedium
                    menu: ContextMenu {
                        MenuItem {
                            text: qsTr("Remove download")
                            onClicked: albumItem.remorseDelete(function() {
                                downloads.removeAlbum(modelData.id)
                            })
                        }
                    }

                    Column {
                        anchors.verticalCenter: parent.verticalCenter
                        x: Theme.horizontalPageMargin
                        width: parent.width - 2 * Theme.horizontalPageMargin

                        Label {
                            width: parent.width
                            truncationMode: TruncationMode.Fade
                            color: albumItem.highlighted ? Theme.highlightColor : Theme.primaryColor
                            text: modelData.title
                        }
                        Label {
                            width: parent.width
                            truncationMode: TruncationMode.Fade
                            font.pixelSize: Theme.fontSizeExtraSmall
                            color: Theme.secondaryColor
                            text: qsTr("%1, tracks: %2").arg(Format.formatFileSize(modelData.bytes)).arg(modelData.tracks)
                        }
                    }
                }
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: page.storage.used > 0 || downloads.waiting > 0
                text: qsTr("Remove all downloads")
                onClicked: remorse.execute(qsTr("Removing all downloads"), function() { downloads.removeAll() })
            }

            SectionHeader { text: qsTr("Download queue") }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                color: downloads.paused ? Theme.errorColor : Theme.highlightColor
                text: downloads.running ? qsTr("Downloading %1 (%2%), waiting: %3")
                                          .arg(downloads.current).arg(downloads.percent).arg(downloads.waiting)
                      : downloads.paused ? qsTr("%1. Waiting: %2. SailTune tries again every minute while it runs.")
                                           .arg(downloads.problem).arg(downloads.waiting)
                      : qsTr("Nothing to download")
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                visible: downloads.paused
                text: qsTr("Try again now")
                onClicked: downloads.resume()
            }

            SectionHeader { text: qsTr("About") }

            DetailItem {
                label: qsTr("Version")
                value: appVersion
            }
            DetailItem {
                label: qsTr("License")
                value: "MIT"
            }
        }

        VerticalScrollDecorator {}
    }
}
