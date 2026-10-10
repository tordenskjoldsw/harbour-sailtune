import QtQuick 2.0
import Sailfish.Silica 1.0
import harbour.sailtune 1.0
import "../components"

Page {
    property string playlistId
    property string playlistName

    allowedOrientations: Orientation.All

    SilicaListView {
        anchors.fill: parent
        header: PageHeader { title: playlistName }

        model: LibraryModel {
            id: tracks

            source: sources.active
            view: LibraryModel.PlaylistTracks
            filter: playlistId
        }

        delegate: TwoLineItem {
            image: model.image
            title: model.title
            description: model.artist
            onClicked: player.playTracks(tracks.rows(), index)
        }

        VerticalScrollDecorator {}
    }
}
