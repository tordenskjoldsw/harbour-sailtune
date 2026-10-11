import QtQuick 2.0
import Sailfish.Silica 1.0
import harbour.sailtune 1.0

// The start page: the active source's library from its cache, never a
// login page. The pulley switches sources, searches and refreshes.
Page {
    id: page

    objectName: "libraryPage"

    readonly property var source: sources.active
    readonly property bool onlyDownloaded: sources.showingDownloadedOnly
    // Read again whenever the library, the downloads or the filter change.
    readonly property var counts: {
        source.trackCount
        downloads.revision
        return source.counts(onlyDownloaded)
    }
    readonly property bool empty: counts.tracks === 0

    allowedOrientations: Orientation.All

    SilicaListView {
        id: list

        anchors.fill: parent

        PullDownMenu {
            busy: page.source.refreshing
            MenuItem {
                text: qsTr("Settings")
                onClicked: pageStack.push(Qt.resolvedUrl("SettingsPage.qml"))
            }
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

        header: Column {
            width: parent.width

            PageHeader {
                title: page.source.name
                description: page.source.refreshing ? qsTr("Updating the library")
                             : page.source.problem.length > 0 ? page.source.problem
                             : page.empty ? "" : qsTr("Tracks: %1").arg(page.counts.tracks)
            }

            Button {
                visible: page.source.needsLogin === true
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Log in again")
                onClicked: pageStack.push(Qt.resolvedUrl("ServerPage.qml"), { "sourceId": page.source.id })
            }

            TextSwitch {
                visible: page.source.canDownload
                automaticCheck: false
                checked: page.onlyDownloaded
                enabled: page.source.reachable
                text: qsTr("Downloaded only")
                description: page.source.reachable
                             ? qsTr("Show only the music that plays without a network")
                             : qsTr("The server cannot be reached, so only downloaded music shows")
                onClicked: sources.downloadedOnly = !sources.downloadedOnly
            }
        }

        model: page.empty ? [] : [
            { "title": qsTr("Artists"), "count": page.counts.artists, "page": "ArtistsPage.qml" },
            { "title": qsTr("Albums"), "count": page.counts.albums, "page": "AlbumsPage.qml" },
            { "title": qsTr("Playlists"), "count": page.counts.playlists, "page": "PlaylistsPage.qml" }
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
            text: page.onlyDownloaded ? qsTr("Nothing downloaded yet")
                  : page.source.kind === "phone" ? qsTr("No music on this phone")
                  : qsTr("No music on this server")
            hintText: page.onlyDownloaded
                      ? qsTr("Download albums or playlists from their pulley menu while the server is reachable.")
                      : page.source.kind === "phone"
                        ? qsTr("Copy music into the Music folder, or add your server under Sources in the pulley menu.")
                        : qsTr("Update the library from the pulley menu.")
        }

        VerticalScrollDecorator {}
    }
}
