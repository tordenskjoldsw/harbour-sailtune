import QtQuick 2.0
import Sailfish.Silica 1.0
import harbour.sailtune 1.0
import "../components"

// Picks the server playlist the tracks go to, or names a new one. The
// page closes once the server has taken them.
Page {
    id: page

    property var trackIds: []
    property bool working
    property bool done
    property string problem

    function add(playlistId) {
        working = true
        problem = ""
        sources.active.addToPlaylist(playlistId, trackIds)
    }

    function create(name) {
        working = true
        problem = ""
        sources.active.createPlaylist(name, trackIds)
    }

    allowedOrientations: Orientation.All
    backNavigation: !working

    // The answer may come while the new-playlist dialog is still closing.
    onStatusChanged: {
        if (status === PageStatus.Active && done)
            pageStack.pop()
    }

    Connections {
        target: sources.active
        onPlaylistEditFinished: {
            if (!page.working)
                return
            page.working = false
            page.problem = problem
            if (problem.length === 0) {
                page.done = true
                if (page.status === PageStatus.Active)
                    pageStack.pop()
            }
        }
    }

    SilicaListView {
        anchors.fill: parent

        header: Column {
            width: parent.width

            PageHeader {
                title: qsTr("Add to playlist")
                description: page.problem.length > 0 ? page.problem
                             : page.trackIds.length === 1 ? qsTr("1 track")
                             : qsTr("%1 tracks").arg(page.trackIds.length)
            }

            BackgroundItem {
                enabled: !page.working
                onClicked: pageStack.push(newPlaylistDialog)

                Label {
                    anchors.verticalCenter: parent.verticalCenter
                    x: Theme.horizontalPageMargin
                    width: parent.width - 2 * x
                    truncationMode: TruncationMode.Fade
                    color: parent.highlighted ? Theme.highlightColor : Theme.primaryColor
                    text: qsTr("New playlist")
                }
            }
        }

        model: LibraryModel {
            id: playlists

            source: sources.active
            view: LibraryModel.Playlists
        }

        delegate: TwoLineItem {
            enabled: !page.working
            image: model.image
            title: model.title
            description: qsTr("Tracks: %1").arg(model.count)
            onClicked: page.add(model.id)
        }

        VerticalScrollDecorator {}
    }

    BusyIndicator {
        anchors.centerIn: parent
        size: BusyIndicatorSize.Large
        running: page.working
    }

    Component {
        id: newPlaylistDialog

        Dialog {
            canAccept: nameField.text.trim().length > 0
            onAccepted: page.create(nameField.text.trim())

            Column {
                width: parent.width

                DialogHeader {
                    acceptText: qsTr("Create")
                    title: qsTr("New playlist")
                }

                TextField {
                    id: nameField

                    width: parent.width
                    focus: true
                    label: qsTr("Name")
                    placeholderText: qsTr("Name")
                    maximumLength: 200
                    EnterKey.enabled: text.trim().length > 0
                    EnterKey.iconSource: "image://theme/icon-m-enter-accept"
                    EnterKey.onClicked: accept()
                }
            }
        }
    }
}
