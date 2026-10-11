import QtQuick 2.0
import Sailfish.Silica 1.0
import "../components/Plain.js" as Plain
import harbour.sailtune 1.0

// Adds a Jellyfin server, or logs in to one again when sourceId is set:
// the address first, then a password or Quick Connect. Only the token is
// kept.
Page {
    id: page

    property string sourceId
    // Inside ServerSetup the name "sources" is its own property.
    readonly property var appSources: sources

    function problemText() {
        switch (setup.problem) {
        case ServerSetup.InvalidAddress: return qsTr("This is no server address")
        case ServerSetup.Unreachable: return qsTr("The server cannot be reached. At home, check the address; away, the VPN.")
        case ServerSetup.NotJellyfin: return qsTr("No Jellyfin server answers at this address")
        case ServerSetup.WrongLogin: return qsTr("User name or password is wrong")
        case ServerSetup.QuickConnectFailed: return qsTr("Quick Connect did not work. Try again, or use the password.")
        case ServerSetup.ServerError: return qsTr("The server refused the login")
        default: return ""
        }
    }

    allowedOrientations: Orientation.All

    ServerSetup {
        id: setup

        sources: page.appSources
        Component.onCompleted: {
            if (page.sourceId.length > 0) {
                sourceId = page.sourceId
                check(address)
            }
        }
        onFinished: {
            sources.active.refresh()
            pageStack.pop(pageStack.find(function(found) { return found.objectName === "libraryPage" }))
        }
    }

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column

            width: parent.width
            spacing: Theme.paddingLarge

            PageHeader {
                title: page.sourceId.length > 0 ? qsTr("Log in again") : qsTr("Add Jellyfin server")
                description: Plain.text(setup.serverName)
            }

            TextField {
                id: addressField

                visible: setup.step === ServerSetup.EnterAddress
                width: parent.width
                label: qsTr("Server address")
                placeholderText: "192.168.1.10:8096"
                inputMethodHints: Qt.ImhUrlCharactersOnly | Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                EnterKey.enabled: text.length > 0
                EnterKey.iconSource: "image://theme/icon-m-enter-next"
                EnterKey.onClicked: setup.check(text)
            }

            Label {
                // Plain HTTP is for a server reached at home or through a
                // VPN only (PLAN.md section 6).
                visible: (setup.step === ServerSetup.EnterAddress && addressField.text.length > 0
                          && addressField.text.trim().toLowerCase().indexOf("https://") !== 0)
                         || (setup.step === ServerSetup.EnterLogin && setup.address.indexOf("http://") === 0)
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryHighlightColor
                text: qsTr("Without https, the password and the access token travel unencrypted. Use this only at home or through a VPN.")
            }

            Button {
                visible: setup.step === ServerSetup.EnterAddress
                anchors.horizontalCenter: parent.horizontalCenter
                enabled: addressField.text.length > 0
                text: qsTr("Continue")
                onClicked: setup.check(addressField.text)
            }

            TextField {
                id: userField

                visible: setup.step === ServerSetup.EnterLogin
                width: parent.width
                label: qsTr("User name")
                inputMethodHints: Qt.ImhNoAutoUppercase | Qt.ImhNoPredictiveText
                EnterKey.iconSource: "image://theme/icon-m-enter-next"
                EnterKey.onClicked: passwordField.focus = true
            }

            PasswordField {
                id: passwordField

                visible: setup.step === ServerSetup.EnterLogin
                width: parent.width
                EnterKey.enabled: userField.text.length > 0
                EnterKey.iconSource: "image://theme/icon-m-enter-accept"
                EnterKey.onClicked: setup.logIn(userField.text, text)
            }

            Button {
                visible: setup.step === ServerSetup.EnterLogin
                anchors.horizontalCenter: parent.horizontalCenter
                enabled: userField.text.length > 0
                text: qsTr("Log in")
                onClicked: setup.logIn(userField.text, passwordField.text)
            }

            Button {
                visible: setup.step === ServerSetup.EnterLogin && setup.quickConnect
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Use Quick Connect")
                onClicked: setup.startQuickConnect()
            }

            Label {
                visible: setup.step === ServerSetup.WaitingForQuickConnect && setup.quickConnectCode.length > 0
                anchors.horizontalCenter: parent.horizontalCenter
                font.pixelSize: Theme.fontSizeHuge
                color: Theme.highlightColor
                text: setup.quickConnectCode
            }

            Label {
                visible: setup.step === ServerSetup.WaitingForQuickConnect
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                color: Theme.secondaryHighlightColor
                text: qsTr("Enter this code in another Jellyfin app where you are logged in, under Quick Connect. SailTune logs in as soon as you allow it.")
            }

            Button {
                visible: setup.step === ServerSetup.WaitingForQuickConnect
                anchors.horizontalCenter: parent.horizontalCenter
                text: qsTr("Cancel")
                onClicked: setup.cancelQuickConnect()
            }

            BusyIndicator {
                anchors.horizontalCenter: parent.horizontalCenter
                size: BusyIndicatorSize.Large
                running: setup.step === ServerSetup.Checking || setup.step === ServerSetup.LoggingIn
                         || setup.step === ServerSetup.WaitingForQuickConnect
                visible: running
            }

            Label {
                visible: text.length > 0
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                color: Theme.errorColor
                text: page.problemText()
            }

            Label {
                visible: setup.step === ServerSetup.EnterLogin
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                wrapMode: Text.Wrap
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
                text: qsTr("The password goes only to the server. SailTune keeps the access token it gets back, which you can revoke on the server.")
            }
        }

        VerticalScrollDecorator {}
    }
}
