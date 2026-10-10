import QtQuick 2.0
import Sailfish.Silica 1.0

// A list row with an image, a title and a line below it.
ListItem {
    id: item

    property alias image: art.source
    property bool showImage: true
    property string title
    property string description

    contentHeight: Theme.itemSizeMedium

    AlbumArt {
        id: art

        visible: item.showImage
        anchors.verticalCenter: parent.verticalCenter
        x: Theme.horizontalPageMargin
        width: Theme.itemSizeMedium - Theme.paddingSmall
    }

    Column {
        anchors.verticalCenter: parent.verticalCenter
        x: item.showImage ? art.x + art.width + Theme.paddingMedium : Theme.horizontalPageMargin
        width: parent.width - x - Theme.horizontalPageMargin

        Label {
            width: parent.width
            truncationMode: TruncationMode.Fade
            color: item.highlighted ? Theme.highlightColor : Theme.primaryColor
            text: item.title
        }
        Label {
            width: parent.width
            truncationMode: TruncationMode.Fade
            font.pixelSize: Theme.fontSizeExtraSmall
            color: item.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
            text: item.description
            visible: text.length > 0
        }
    }
}
