import QtQuick 2.0
import Sailfish.Silica 1.0
import harbour.sailtune 1.0

// The start page: the active source's library from its cache, never a
// login page. The pulley switches sources, searches and refreshes.
Page {
    id: page

    readonly property var source: sources.active
    readonly property bool empty: source.trackCount === 0

    allowedOrientations: Orientation.All

    SilicaListView {
        id: list

        anchors.fill: parent

        PullDownMenu {
            busy: page.source.refreshing
            MenuItem {
                text: qsTr("Sources")
                onClicked: pageStack.push(Qt.resolvedUrl("SourcesPage.qml"))
            }
            MenuItem {
                text: qsTr("Update library")
                enabled: !page.source.refreshing
                onClicked: page.source.refresh()
            }
            MenuItem {
                text: qsTr("Search")
                visible: !page.empty
                onClicked: pageStack.push(Qt.resolvedUrl("SearchPage.qml"))
            }
        }

        header: PageHeader {
            title: page.source.name
            description: page.source.refreshing ? qsTr("Updating the library")
                         : page.source.problem.length > 0 ? page.source.problem
                         : page.empty ? "" : qsTr("Tracks: %1").arg(page.source.trackCount)
        }

        model: page.empty ? [] : [
            { "title": qsTr("Artists"), "count": page.source.artistCount, "page": "ArtistsPage.qml" },
            { "title": qsTr("Albums"), "count": page.source.albumCount, "page": "AlbumsPage.qml" },
            { "title": qsTr("Playlists"), "count": page.source.playlistCount, "page": "PlaylistsPage.qml" }
        ]

        delegate: ListItem {
            onClicked: pageStack.push(Qt.resolvedUrl(modelData.page))

            Label {
                anchors.verticalCenter: parent.verticalCenter
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin - countLabel.width
                truncationMode: TruncationMode.Fade
                color: highlighted ? Theme.highlightColor : Theme.primaryColor
                text: modelData.title
            }
            Label {
                id: countLabel

                anchors {
                    verticalCenter: parent.verticalCenter
                    right: parent.right
                    rightMargin: Theme.horizontalPageMargin
                }
                color: Theme.secondaryColor
                text: modelData.count
            }
        }

        ViewPlaceholder {
            enabled: page.empty && !page.source.refreshing
            text: page.source.kind === "phone" ? qsTr("No music on this phone")
                                               : qsTr("No music on this server")
            hintText: page.source.kind === "phone"
                      ? qsTr("Copy music into the Music folder, or add your server under Sources in the pulley menu.")
                      : qsTr("Update the library from the pulley menu.")
        }

        VerticalScrollDecorator {}
    }
}
