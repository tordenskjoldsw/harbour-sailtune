import QtQuick 2.0
import Sailfish.Silica 1.0

// A placeholder that only checks whether the Jolla Store accepts the
// package name harbour-sailtune.
ApplicationWindow {
    initialPage: Component {
        Page {
            PageHeader { title: "SailTune" }

            Label {
                anchors.centerIn: parent
                width: parent.width - 2 * Theme.horizontalPageMargin
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                color: Theme.highlightColor
                text: qsTr("Name check build of SailTune, a music player for Jellyfin.")
            }
        }
    }
    cover: Component {
        CoverBackground {
            Label { anchors.centerIn: parent; text: "SailTune" }
        }
    }
}
