import QtQuick 2.0
import Sailfish.Silica 1.0

// A list row with an image, a title and a line below it.
ListItem {
    id: item

    property alias image: art.source
    property bool showImage: true
    property string title
    property string description
    // Marks what plays without a network, with a check at the right edge.
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

    HighlightImage {
        id: check

        visible: item.downloaded
        anchors {
            right: parent.right
            rightMargin: Theme.horizontalPageMargin
            verticalCenter: parent.verticalCenter
        }
        width: Theme.iconSizeSmall
        height: width
        source: "image://theme/icon-s-checkmark"
        highlighted: item.highlighted
    }

    Column {
        anchors.verticalCenter: parent.verticalCenter
        x: item.showImage ? art.x + art.width + Theme.paddingMedium : Theme.horizontalPageMargin
        width: parent.width - x - Theme.horizontalPageMargin
               - (item.downloaded ? check.width + Theme.paddingMedium : 0)

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
            visible: item.description.length > 0 || item.favorite

            HighlightImage {
                visible: item.favorite
                anchors.verticalCenter: parent.verticalCenter
                width: Theme.iconSizeExtraSmall
                height: width
                source: "image://theme/icon-s-favorite"
                highlighted: item.highlighted
            }
            Label {
                width: parent.width - (item.favorite ? Theme.iconSizeExtraSmall + parent.spacing : 0)
                truncationMode: TruncationMode.Fade
                font.pixelSize: Theme.fontSizeExtraSmall
                color: item.highlighted ? Theme.secondaryHighlightColor : Theme.secondaryColor
                textFormat: Text.PlainText
                text: item.description
            }
        }
    }
}
