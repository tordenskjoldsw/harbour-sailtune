import QtQuick 2.0
import Sailfish.Silica 1.0
import harbour.sailtune 1.0

// The sources the app knows; a tap makes one the active source.
Page {
    allowedOrientations: Orientation.All

    SilicaListView {
        anchors.fill: parent
        header: PageHeader { title: qsTr("Sources") }

        model: sources.all

        delegate: ListItem {
            onClicked: {
                sources.activate(modelData.id)
                sources.active.refresh()
                pageStack.pop()
            }

            Label {
                anchors.verticalCenter: parent.verticalCenter
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                truncationMode: TruncationMode.Fade
                color: modelData.active || highlighted ? Theme.highlightColor : Theme.primaryColor
                text: modelData.name
            }
        }

        VerticalScrollDecorator {}
    }
}
