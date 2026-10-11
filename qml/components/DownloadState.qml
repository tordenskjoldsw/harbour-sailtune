import QtQuick 2.0
import Sailfish.Silica 1.0

// How much of a list of tracks is downloaded, and what the downloads are
// doing right now.
Label {
    property var tracks

    visible: sources.active.canDownload && tracks.count > 0
    x: Theme.horizontalPageMargin
    width: parent.width - 2 * Theme.horizontalPageMargin
    wrapMode: Text.Wrap
    font.pixelSize: Theme.fontSizeExtraSmall
    color: downloads.problem.length > 0 ? Theme.errorColor : Theme.secondaryHighlightColor
    textFormat: Text.PlainText
    text: downloads.paused ? qsTr("%1. Waiting: %2").arg(downloads.problem).arg(downloads.waiting)
          : downloads.problem.length > 0 ? downloads.problem
          : downloads.running ? qsTr("Downloading %1 (%2%), waiting: %3")
                                .arg(downloads.current).arg(downloads.percent).arg(downloads.waiting)
          : qsTr("Downloaded: %1 of %2").arg(tracks.downloadedCount).arg(tracks.count)
}
