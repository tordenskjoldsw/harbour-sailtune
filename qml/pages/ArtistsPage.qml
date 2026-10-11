import QtQuick 2.0
import Sailfish.Silica 1.0
import harbour.sailtune 1.0
import "../components"

Page {
    allowedOrientations: Orientation.All

    SilicaListView {
        anchors.fill: parent
        header: PageHeader { title: qsTr("Artists") }

        model: LibraryModel {
            source: sources.active
            downloadedOnly: sources.showingDownloadedOnly
            revision: downloads.revision
            view: LibraryModel.Artists
        }

        delegate: TwoLineItem {
            image: model.image
            title: model.title
            description: qsTr("Albums: %1").arg(model.count)
            onClicked: pageStack.push(Qt.resolvedUrl("ArtistPage.qml"),
                                      { "artistId": model.id, "artistName": model.title })
        }

        VerticalScrollDecorator {}
    }
}
