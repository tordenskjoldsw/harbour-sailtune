import QtQuick 2.0
import Sailfish.Silica 1.0
import harbour.sailtune 1.0
import "../components"

// Artists, albums and tracks whose name contains the search text.
Page {
    allowedOrientations: Orientation.All

    LibraryModel {
        id: artists

        source: sources.active
        view: LibraryModel.SearchArtists
        filter: field.text
    }
    LibraryModel {
        id: albums

        source: sources.active
        view: LibraryModel.SearchAlbums
        filter: field.text
    }
    LibraryModel {
        id: tracks

        source: sources.active
        view: LibraryModel.SearchTracks
        filter: field.text
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height

        Column {
            id: column

            width: parent.width

            PageHeader { title: qsTr("Search") }

            SearchField {
                id: field

                width: parent.width
                placeholderText: qsTr("Artist, album or track")
                Component.onCompleted: forceActiveFocus()
                EnterKey.onClicked: focus = false
            }

            SectionHeader {
                visible: artists.count > 0
                text: qsTr("Artists")
            }
            Repeater {
                model: artists
                TwoLineItem {
                    width: column.width
                    image: model.image
                    title: model.title
                    onClicked: pageStack.push(Qt.resolvedUrl("ArtistPage.qml"),
                                              { "artistId": model.id, "artistName": model.title })
                }
            }

            SectionHeader {
                visible: albums.count > 0
                text: qsTr("Albums")
            }
            Repeater {
                model: albums
                TwoLineItem {
                    width: column.width
                    image: model.image
                    title: model.title
                    description: model.artist
                    onClicked: pageStack.push(Qt.resolvedUrl("AlbumPage.qml"),
                                              { "albumId": model.id, "albumTitle": model.title,
                                                "albumArtist": model.artist, "albumImage": model.image })
                }
            }

            SectionHeader {
                visible: tracks.count > 0
                text: qsTr("Tracks")
            }
            Repeater {
                model: tracks
                TwoLineItem {
                    width: column.width
                    image: model.image
                    title: model.title
                    description: model.artist + " - " + model.album
                    onClicked: player.playTracks(tracks.rows(), index)
                }
            }
        }

        VerticalScrollDecorator {}
    }
}
