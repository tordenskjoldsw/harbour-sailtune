import QtQuick 2.0
import Sailfish.Silica 1.0
import "../components"

Page {
    id: page

    readonly property string repository: "https://github.com/tordenskjoldsw/harbour-sailtune"

    allowedOrientations: Orientation.All

    SilicaFlickable {
        anchors.fill: parent
        contentHeight: column.height + Theme.paddingLarge

        Column {
            id: column

            width: parent.width
            spacing: Theme.paddingLarge

            PageHeader {
                title: qsTr("About")
            }

            Image {
                anchors.horizontalCenter: parent.horizontalCenter
                width: Theme.iconSizeExtraLarge
                height: width
                sourceSize { width: width; height: height }
                source: "/usr/share/icons/hicolor/172x172/apps/harbour-sailtune.png"
            }

            Column {
                width: parent.width

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    textFormat: Text.PlainText
                    font.pixelSize: Theme.fontSizeLarge
                    color: Theme.highlightColor
                    text: "SailTune"
                }

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    textFormat: Text.PlainText
                    font.pixelSize: Theme.fontSizeSmall
                    color: Theme.secondaryHighlightColor
                    text: qsTr("Version %1").arg(appVersion)
                }

                Label {
                    anchors.horizontalCenter: parent.horizontalCenter
                    visible: appBuild.length > 0
                    textFormat: Text.PlainText
                    font.pixelSize: Theme.fontSizeExtraSmall
                    color: Theme.secondaryColor
                    text: qsTr("Development build %1").arg(appBuild)
                }
            }

            Repeater {
                model: [
                    qsTr("I built SailTune to play my music on my Sailfish phone: the music on the phone itself and the music on my own Jellyfin server, streamed at home or over a VPN and downloaded for the road."),
                    qsTr("Offline comes first. Downloaded music plays without any network and without a login, and the app never opens on a login page. SailTune talks only to the servers you add: no account, no analytics, no advertising."),
                    qsTr("SailTune is free software under the MIT license. Read the code, check what I claim here and tell me what you find.")
                ]

                Paragraph {
                    color: Theme.highlightColor
                    text: modelData
                }
            }

            Column {
                width: parent.width

                Repeater {
                    model: [
                        { "text": qsTr("Source code"), "url": page.repository },
                        { "text": qsTr("Privacy policy"),
                          "url": page.repository + "/blob/main/PRIVACY.md" },
                        { "text": qsTr("Security review"),
                          "url": page.repository + "/blob/main/docs/security-review-2026-10.md" },
                        { "text": qsTr("Report a problem"), "url": page.repository + "/issues" },
                        { "text": qsTr("Report a security issue privately"),
                          "url": page.repository + "/security/advisories/new" }
                    ]

                    BackgroundItem {
                        id: link

                        width: parent.width
                        onClicked: Qt.openUrlExternally(modelData.url)

                        Label {
                            x: Theme.horizontalPageMargin
                            width: parent.width - 2 * Theme.horizontalPageMargin
                            anchors.verticalCenter: parent.verticalCenter
                            textFormat: Text.PlainText
                            truncationMode: TruncationMode.Fade
                            color: link.highlighted ? Theme.highlightColor : Theme.primaryColor
                            text: modelData.text
                        }
                    }
                }
            }

            Paragraph {
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
                text: qsTr("Copyright 2026 Tobias Kaminski. SailTune is an independent project and not affiliated with Jellyfin or Jolla.")
            }
        }

        VerticalScrollDecorator {}
    }
}
