import QtQuick 2.0
import Sailfish.Silica 1.0
import Amber.Mpris 1.0
import "pages"
import "components"

ApplicationWindow {
    id: app

    initialPage: Component { LibraryPage {} }
    cover: Qt.resolvedUrl("cover/CoverPage.qml")
    allowedOrientations: defaultAllowedOrientations
    bottomMargin: miniPlayer.visibleSize

    // The cache shows at once; the active source refreshes behind it.
    Component.onCompleted: sources.active.refresh()

    function updateMetaData() {
        mpris.metaData.title = player.title
        mpris.metaData.contributingArtist = player.artist.length > 0 ? [ player.artist ] : []
        mpris.metaData.albumTitle = player.album
        mpris.metaData.artUrl = player.image
        // MPRIS counts in microseconds.
        mpris.metaData.duration = player.duration * 1000
    }

    Connections {
        target: player
        onTrackChanged: app.updateMetaData()
        onDurationChanged: app.updateMetaData()
    }

    MprisPlayer {
        id: mpris

        serviceName: "sailtune"
        identity: "SailTune"
        canControl: true
        canPlay: player.hasTrack
        canPause: player.hasTrack
        canGoNext: player.queueIndex + 1 < player.queueLength
        canGoPrevious: player.hasTrack
        canSeek: player.seekable
        playbackStatus: player.playing ? Mpris.Playing
                      : player.hasTrack ? Mpris.Paused : Mpris.Stopped
        position: player.position * 1000

        onPlayRequested: player.play()
        onPauseRequested: player.pause()
        onPlayPauseRequested: player.togglePlay()
        onNextRequested: player.next()
        onPreviousRequested: player.previous()
        onStopRequested: player.pause()
        onSeekRequested: player.seekBy(offset / 1000)
        onSetPositionRequested: player.seek(position / 1000)
    }

    MiniPlayer {
        id: miniPlayer
    }
}
