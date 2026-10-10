import QtQuick 2.0
import Sailfish.Silica 1.0

// The first page until the server login of Phase 2 takes its place.
Page {
    allowedOrientations: Orientation.All

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height

        Column {
            id: column

            width: parent.width
            spacing: Theme.paddingLarge

            PageHeader {
                title: qsTr("SailTune")
                description: qsTr("Version %1").arg(appVersion)
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                color: Theme.highlightColor
                text: qsTr("Music from your own server, streamed or downloaded to play without a network.")
            }
        }
    }
}
