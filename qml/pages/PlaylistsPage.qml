import QtQuick 2.0
import Sailfish.Silica 1.0
import harbour.sailtune 1.0
import "../components"

Page {
    allowedOrientations: Orientation.All

    SilicaListView {
        id: list

        anchors.fill: parent
        header: PageHeader { title: qsTr("Playlists") }

        model: LibraryModel {
            id: playlists

            source: sources.active
            view: LibraryModel.Playlists
        }

        delegate: TwoLineItem {
            image: model.image
            title: model.title
            description: qsTr("Tracks: %1").arg(model.count)
            onClicked: pageStack.push(Qt.resolvedUrl("PlaylistPage.qml"),
                                      { "playlistId": model.id, "playlistName": model.title })
        }

        ViewPlaceholder {
            enabled: playlists.count === 0
            text: qsTr("No playlists")
        }

        VerticalScrollDecorator {}
    }
}
