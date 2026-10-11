import QtQuick 2.0
import Sailfish.Silica 1.0
import harbour.sailtune 1.0

// The sources the app knows; a tap makes one the active source. Servers
// can log in again or be removed from their menu.
Page {
    allowedOrientations: Orientation.All

    SilicaListView {
        anchors.fill: parent
        header: PageHeader { title: qsTr("Sources") }

        PullDownMenu {
            MenuItem {
                text: qsTr("Add Jellyfin server")
                onClicked: pageStack.push(Qt.resolvedUrl("ServerPage.qml"))
            }
        }

        model: sources.all

        delegate: ListItem {
            id: item

            contentHeight: Theme.itemSizeMedium
            menu: modelData.kind === "jellyfin" ? serverMenu : null
            onClicked: {
                sources.activate(modelData.id)
                sources.active.refresh()
                pageStack.pop()
            }

            // Downloads of gigabytes come back only with time and the
            // server, so a server that has some asks first and says how
            // much goes; one without them has the usual countdown.
            function remove() {
                var bytes = downloads.sourceBytes(modelData.id)
                if (bytes > 0) {
                    pageStack.push(removeDialog, { "sourceId": modelData.id, "sourceName": modelData.name,
                                                   "bytes": bytes })
                } else {
                    remorseDelete(function() { sources.remove(modelData.id) })
                }
            }

            Column {
                anchors.verticalCenter: parent.verticalCenter
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin

                Label {
                    width: parent.width
                    truncationMode: TruncationMode.Fade
                    color: modelData.active || item.highlighted ? Theme.highlightColor : Theme.primaryColor
                    textFormat: Text.PlainText
                    text: modelData.name
                }
                Label {
                    width: parent.width
                    truncationMode: TruncationMode.Fade
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: modelData.needsLogin ? Theme.errorColor : Theme.secondaryColor
                    textFormat: Text.PlainText
                    text: modelData.needsLogin ? qsTr("Log in again from the menu")
                                               : modelData.kind === "phone" ? qsTr("Music on this phone")
                                                                            : modelData.detail
                }
            }

            Component {
                id: serverMenu

                ContextMenu {
                    MenuItem {
                        text: qsTr("Log in again")
                        onClicked: pageStack.push(Qt.resolvedUrl("ServerPage.qml"),
                                                  { "sourceId": modelData.id })
                    }
                    MenuItem {
                        text: qsTr("Remove")
                        onClicked: item.remove()
                    }
                }
            }
        }

        VerticalScrollDecorator {}
    }

    Component {
        id: removeDialog

        Dialog {
            property string sourceId
            property string sourceName
            property real bytes

            onAccepted: sources.remove(sourceId)

            Column {
                width: parent.width
                spacing: Theme.paddingLarge

                DialogHeader {
                    acceptText: qsTr("Remove")
                    title: qsTr("Remove server?")
                }

                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * Theme.horizontalPageMargin
                    wrapMode: Text.Wrap
                    color: Theme.highlightColor
                    textFormat: Text.PlainText
                    text: qsTr("%1 and its downloaded music, %2, are removed from this phone. The music can be downloaded again only with a connection to the server.")
                          .arg(sourceName).arg(Format.formatFileSize(bytes))
                }
                Label {
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * Theme.horizontalPageMargin
                    wrapMode: Text.Wrap
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.secondaryHighlightColor
                    text: qsTr("To keep the music, keep the server; when its login has expired, log in again instead.")
                }
            }
        }
    }
}
