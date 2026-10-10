import QtQuick 2.0
import Sailfish.Silica 1.0

Page {
    id: page

    allowedOrientations: Orientation.All

    Connections {
        target: jellyfin
        onLoggedInChanged: {
            if (jellyfin.loggedIn)
                pageStack.replace(Qt.resolvedUrl("AlbumsPage.qml"))
        }
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        PullDownMenu {
            MenuItem {
                text: "Log"
                onClicked: pageStack.push(Qt.resolvedUrl("LogPage.qml"))
            }
            MenuItem {
                text: "Downloaded music"
                onClicked: pageStack.push(Qt.resolvedUrl("OfflinePage.qml"))
            }
        }

        Column {
            id: column

            width: parent.width
            spacing: Theme.paddingMedium

            PageHeader { title: "Jellyfin login" }

            TextField {
                id: server

                width: parent.width
                label: "Server"
                placeholderText: "http://192.168.1.10:8096"
                inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoAutoUppercase
                EnterKey.onClicked: user.focus = true
            }

            TextField {
                id: user

                width: parent.width
                label: "User"
                placeholderText: label
                inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                EnterKey.onClicked: password.focus = true
            }

            PasswordField {
                id: password

                width: parent.width
                EnterKey.onClicked: jellyfin.login(server.text, user.text, password.text)
            }

            Button {
                anchors.horizontalCenter: parent.horizontalCenter
                enabled: !jellyfin.busy && server.text.length > 0 && user.text.length > 0
                text: "Log in"
                onClicked: jellyfin.login(server.text, user.text, password.text)
            }

            BusyIndicator {
                anchors.horizontalCenter: parent.horizontalCenter
                running: jellyfin.busy
                visible: running
            }

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                color: Theme.errorColor
                text: jellyfin.error
            }
        }
    }
}
