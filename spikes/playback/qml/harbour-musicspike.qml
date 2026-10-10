import QtQuick 2.0
import QtMultimedia 5.6
import Sailfish.Silica 1.0
import Amber.Mpris 1.0

// One player for the whole spike: a queue of tracks, streamed or played
// from downloaded files, with lock screen controls and a log of what
// happens while the screen is off.
ApplicationWindow {
    id: app

    property var queue: []
    property int queueIndex: -1
    readonly property var current: queueIndex >= 0 && queueIndex < queue.length ? queue[queueIndex] : null
    property int loggedMinute: -1

    function time(milliseconds) {
        var seconds = Math.floor(milliseconds / 1000)
        var rest = seconds % 60
        return Math.floor(seconds / 60) + ":" + (rest < 10 ? "0" : "") + rest
    }

    // tracks: maps from Jellyfin.tracksLoaded; offline plays the
    // downloaded files.
    function playQueue(tracks, index, offline) {
        var list = []
        for (var i = 0; i < tracks.length; ++i) {
            var track = tracks[i]
            list.push({
                "id": track.id, "name": track.name, "artist": track.artist,
                "album": track.album, "seconds": track.seconds, "codec": track.codec,
                "offline": offline,
                "url": offline ? jellyfin.localFile(track.id, track.container)
                               : jellyfin.streamUrl(track.id)
            })
        }
        queue = list
        playIndex(index)
    }

    function playIndex(index) {
        if (index < 0 || index >= queue.length) {
            player.stop()
            jellyfin.log("queue ended")
            return
        }
        queueIndex = index
        loggedMinute = -1
        var track = queue[index]
        player.source = track.url
        player.play()
        // The URL holds the access token, so only the kind goes to the log.
        jellyfin.log("play " + (index + 1) + "/" + queue.length + " " + track.codec + " "
                     + (track.offline ? "from file" : "streamed"))
        mpris.metaData.title = track.name
        mpris.metaData.contributingArtist = [ track.artist ]
        mpris.metaData.albumTitle = track.album
        mpris.metaData.duration = track.seconds * 1000000
    }

    function next() {
        playIndex(queueIndex + 1)
    }

    function previous() {
        playIndex(Math.max(0, queueIndex - 1))
    }

    // milliseconds from the start of the track, kept inside it.
    function seekTo(milliseconds) {
        if (!player.seekable)
            return
        var target = Math.max(0, Math.min(milliseconds, Math.max(0, player.duration - 1000)))
        jellyfin.log("seek from " + time(player.position) + " to " + time(target))
        player.seek(target)
    }

    function togglePlay() {
        if (player.playbackState === Audio.PlayingState)
            player.pause()
        else
            player.play()
    }

    Audio {
        id: player

        audioRole: Audio.MusicRole
        autoLoad: true

        onStatusChanged: {
            var names = {}
            names[Audio.NoMedia] = "no media"
            names[Audio.Loading] = "loading"
            names[Audio.Loaded] = "loaded"
            names[Audio.Buffering] = "buffering"
            names[Audio.Stalled] = "stalled"
            names[Audio.Buffered] = "buffered"
            names[Audio.EndOfMedia] = "end of media"
            names[Audio.InvalidMedia] = "invalid media"
            names[Audio.UnknownStatus] = "unknown"
            if (status === Audio.Stalled || status === Audio.EndOfMedia || status === Audio.InvalidMedia
                    || status === Audio.Buffering)
                jellyfin.log("status: " + names[status])
        }
        onError: jellyfin.log("player error " + error + ": " + errorString)
        onPlaybackStateChanged: {
            jellyfin.log("state: " + (playbackState === Audio.PlayingState ? "playing"
                                      : playbackState === Audio.PausedState ? "paused" : "stopped"))
            // The player reports the end of a track before it stops; a
            // track started on the end report was stopped again.
            if (playbackState === Audio.StoppedState && status === Audio.EndOfMedia)
                app.next()
        }
        // One line per minute of music: gaps against the wall clock show
        // whether playback stalled while the phone slept.
        onPositionChanged: {
            var minute = Math.floor(position / 60000)
            if (minute > app.loggedMinute) {
                app.loggedMinute = minute
                if (minute > 0)
                    jellyfin.log("track " + (app.queueIndex + 1) + " at " + app.time(position))
            }
        }
    }

    Connections {
        target: Qt.application
        onStateChanged: jellyfin.log(Qt.application.state === Qt.ApplicationActive
                                     ? "app in the foreground" : "app in the background")
    }

    MprisPlayer {
        id: mpris

        serviceName: "musicspike"
        identity: "Music spike"
        canControl: true
        canPlay: app.current !== null
        canPause: app.current !== null
        canGoNext: app.queueIndex + 1 < app.queue.length
        canGoPrevious: app.queueIndex > 0
        canSeek: player.seekable
        // MPRIS counts in microseconds.
        position: player.position * 1000
        playbackStatus: player.playbackState === Audio.PlayingState ? Mpris.Playing
                      : player.playbackState === Audio.PausedState ? Mpris.Paused : Mpris.Stopped

        onPlayRequested: { jellyfin.log("lock screen: play"); player.play() }
        onPauseRequested: { jellyfin.log("lock screen: pause"); player.pause() }
        onPlayPauseRequested: { jellyfin.log("lock screen: play/pause"); app.togglePlay() }
        onNextRequested: { jellyfin.log("lock screen: next"); app.next() }
        onPreviousRequested: { jellyfin.log("lock screen: previous"); app.previous() }
        onStopRequested: { jellyfin.log("lock screen: stop"); player.stop() }
        onSeekRequested: {
            jellyfin.log("lock screen: seek by " + Math.round(offset / 1000000) + " s")
            app.seekTo(player.position + offset / 1000)
        }
        onSetPositionRequested: {
            jellyfin.log("lock screen: set position")
            app.seekTo(position / 1000)
        }
    }

    bottomMargin: playerPanel.visibleSize

    DockedPanel {
        id: playerPanel

        width: parent.width
        height: playerColumn.height + 2 * Theme.paddingMedium
        dock: Dock.Bottom
        open: app.current !== null

        Column {
            id: playerColumn

            anchors.verticalCenter: parent.verticalCenter
            width: parent.width

            Label {
                x: Theme.horizontalPageMargin
                width: parent.width - 2 * Theme.horizontalPageMargin
                truncationMode: TruncationMode.Fade
                text: app.current ? app.current.name + " - " + app.current.artist : ""
            }

            Label {
                x: Theme.horizontalPageMargin
                font.pixelSize: Theme.fontSizeExtraSmall
                color: Theme.secondaryColor
                text: app.current
                      ? app.time(player.position) + " / " + app.time(app.current.seconds * 1000)
                        + "  " + (app.current.offline ? "file" : "stream") + "  " + app.current.codec
                        + (player.errorString.length > 0 ? "  " + player.errorString : "")
                      : ""
            }

            Slider {
                id: positionSlider

                width: parent.width
                minimumValue: 0
                maximumValue: Math.max(1, player.duration)
                enabled: player.seekable
                valueText: app.time(value)
                onReleased: app.seekTo(value)

                // Follows the music unless the user drags it.
                Connections {
                    target: player
                    onPositionChanged: {
                        if (!positionSlider.down)
                            positionSlider.value = player.position
                    }
                }
            }

            Row {
                anchors.horizontalCenter: parent.horizontalCenter
                spacing: Theme.paddingMedium

                IconButton {
                    icon.source: "image://theme/icon-m-previous"
                    onClicked: app.previous()
                }
                IconButton {
                    icon.source: "image://theme/icon-m-10s-back"
                    enabled: player.seekable
                    onClicked: app.seekTo(player.position - 10000)
                }
                IconButton {
                    icon.source: player.playbackState === Audio.PlayingState
                                 ? "image://theme/icon-m-pause" : "image://theme/icon-m-play"
                    onClicked: app.togglePlay()
                }
                IconButton {
                    icon.source: "image://theme/icon-m-10s-forward"
                    enabled: player.seekable
                    onClicked: app.seekTo(player.position + 10000)
                }
                IconButton {
                    icon.source: "image://theme/icon-m-next"
                    onClicked: app.next()
                }
            }
        }
    }

    initialPage: Component { LoginPage {} }

    cover: Component {
        CoverBackground {
            Label {
                anchors.centerIn: parent
                width: parent.width - 2 * Theme.paddingLarge
                horizontalAlignment: Text.AlignHCenter
                wrapMode: Text.Wrap
                text: app.current ? app.current.name : "Music spike"
            }

            CoverActionList {
                enabled: app.current !== null

                CoverAction {
                    iconSource: player.playbackState === Audio.PlayingState
                                ? "image://theme/icon-cover-pause" : "image://theme/icon-cover-play"
                    onTriggered: app.togglePlay()
                }
                CoverAction {
                    iconSource: "image://theme/icon-cover-next-song"
                    onTriggered: app.next()
                }
            }
        }
    }
}
