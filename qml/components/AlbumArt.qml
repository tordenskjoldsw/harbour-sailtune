import QtQuick 2.0
import Sailfish.Silica 1.0

// An album's or artist's image, or a music note while there is none.
Item {
    property url source

    width: Theme.itemSizeMedium
    height: width

    Rectangle {
        anchors.fill: parent
        color: Theme.rgba(Theme.highlightBackgroundColor, Theme.highlightBackgroundOpacity)
        visible: image.status !== Image.Ready
    }

    HighlightImage {
        anchors.centerIn: parent
        source: "image://theme/icon-m-media-songs"
        visible: image.status !== Image.Ready
    }

    Image {
        id: image

        anchors.fill: parent
        source: parent.source
        sourceSize.width: width
        sourceSize.height: height
        fillMode: Image.PreserveAspectCrop
        asynchronous: true
    }
}
