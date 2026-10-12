import QtQuick 2.0
import Sailfish.Silica 1.0
import harbour.sailtune 1.0

// The space the downloads use, the state of the download queue, and the
// way to the About page.
Page {
    id: page

    property var storage: downloads.storage()

    allowedOrientations: Orientation.All

    Connections {
        target: downloads
        onRevisionChanged: page.storage = downloads.storage()
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
                      : downloads.paused ? qsTr("%1. Waiting: %2. SailTune tries again as soon as the phone is online, and every 30 seconds while it runs.")
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

            BackgroundItem {
                id: aboutItem

                width: parent.width
                onClicked: pageStack.push(Qt.resolvedUrl("AboutPage.qml"))

                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * Theme.horizontalPageMargin
                    anchors.verticalCenter: parent.verticalCenter
                    textFormat: Text.PlainText
                    truncationMode: TruncationMode.Fade
                    color: aboutItem.highlighted ? Theme.highlightColor : Theme.primaryColor
                    text: qsTr("About SailTune")
                }
            }
        }

        VerticalScrollDecorator {}
    }
}
