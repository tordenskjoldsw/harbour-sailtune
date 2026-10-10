import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    allowedOrientations: Orientation.All

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height

        PullDownMenu {
            MenuItem {
                text: "Clear"
                onClicked: { jellyfin.clearLog(); logText.text = "" }
            }
            MenuItem {
                text: "Refresh"
                onClicked: logText.text = jellyfin.logText()
            }
        }

        Column {
            id: column

            width: parent.width

            PageHeader { title: "Log" }

            Label {
                id: logText

                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                text: jellyfin.logText()
            }
        }

        VerticalScrollDecorator {}
    }
}
