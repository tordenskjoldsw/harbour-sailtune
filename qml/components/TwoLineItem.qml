import QtQuick 2.0
import Sailfish.Silica 1.0

// A list row with an image, a title and a line below it.
ListItem {
    id: item

    property alias image: art.source
    property bool showImage: true
    property string title
    property string description
    // Marks a track that plays without a network.
    property bool downloaded
    property bool favorite

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
            textFormat: Text.PlainText
            text: item.title
        }
        Row {
            width: parent.width
            spacing: Theme.paddingSmall
            visible: item.description.length > 0 || item.downloaded || item.favorite

            HighlightImage {
                visible: item.favorite
                anchors.verticalCenter: parent.verticalCenter
                width: Theme.iconSizeExtraSmall
                height: width
                source: "image://theme/icon-s-favorite"
                highlighted: item.highlighted
            }
            HighlightImage {
                visible: item.downloaded
                anchors.verticalCenter: parent.verticalCenter
                width: Theme.iconSizeExtraSmall
                height: width
                source: "image://theme/icon-s-device-download"
                highlighted: item.highlighted
            }
            Label {
                width: parent.width - (item.downloaded ? Theme.iconSizeExtraSmall + parent.spacing : 0)
                       - (item.favorite ? Theme.iconSizeExtraSmall + parent.spacing : 0)
                truncationMode: TruncationMode.Fade
                font.pixelSize: Theme.fontSizeExtraSmall
                color: item.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
                textFormat: Text.PlainText
                text: item.description
            }
        }
    }
}
