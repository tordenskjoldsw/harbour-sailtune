import QtQuick 2.0
import Sailfish.Silica 1.0

// The long-press menu of a track in a list.
ContextMenu {
    property string trackId
    property bool favorite

    MenuItem {
        text: favorite ? qsTr("Remove from favorites") : qsTr("Add to favorites")
        onClicked: sources.active.setFavorite(trackId, !favorite)
    }
    MenuItem {
        visible: sources.active.canEditPlaylists
        // Playlists change on the server only.
        enabled: sources.active.reachable && sources.active.needsLogin !== true
        text: qsTr("Add to playlist")
        onClicked: pageStack.push(Qt.resolvedUrl("../pages/PlaylistPickerPage.qml"), { "trackIds": [trackId] })
    }
}
