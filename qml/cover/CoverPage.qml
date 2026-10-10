import QtQuick 2.0
import Sailfish.Silica 1.0

CoverBackground {
    Label {
        anchors.centerIn: parent
        width: parent.width - 2 * Theme.paddingLarge
        horizontalAlignment: Text.AlignHCenter
        wrapMode: Text.Wrap
        text: sources.active ? sources.active.name : qsTr("SailTune")
    }
}
