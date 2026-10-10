# Spike: music from Jellyfin

Question: can a Jolla Store app stream and download music from the
maintainer's Jellyfin server, play it for an hour with the screen off,
and be controlled from the lock screen?

The app logs in with user name and password
(`/Users/AuthenticateByName`), lists albums and tracks (`/Items` with
`userId`), and plays a queue with QtMultimedia 5.6 (`Audio`, music role):
either streamed (`/Audio/{id}/stream?static=true`, original file, token
in the URL since QtMultimedia sends no headers) or from files downloaded
with QNetworkAccessManager into the app's private storage. Amber.Mpris
publishes the player for the lock screen; the cover has play/pause and
next. `log.txt` in `~/.local/share/de.tordenskjold/musicspike/` records
logins, downloads (size, time), track changes, buffering and stalls, one
line per minute of music, and lock screen commands; never the token or
a URL.

Sailjail permissions: Audio, Internet.

## Phone

GStreamer plugins on the Jolla Phone (Sailfish OS 5.2.0.18): flac,
mpg123, opus, vorbis, libav (AAC), isomp4, matroska, ogg, soup (HTTP),
pulseaudio. Amber.Mpris 1.0 is installed.

## Validator

`sfdk check` (SailfishOS-5.1.0.11-aarch64, 2026-10-10): QtMultimedia,
Amber.Mpris and the Audio and Internet permissions pass; the missing
icon is the only failure.

## Tests

| # | Test | Result |
|---|------|--------|
| 1 | Log in, list albums, open an album | works (Jellyfin 12.2.0, http, 1 test album) |
| 2 | Stream a track; formats in the library (FLAC, MP3, AAC, Opus) | MP3 streams and plays to its end (15 s test file); other formats open |
| 3 | Download an album; speed in the log | works: 27 FLAC tracks, 900 MB, about 40 MB/s in the home network |
| 4 | Play the album from the downloads with networking off | works from 0.0.4: started without login and WLAN, played from the files |
| 5 | Screen off, phone untouched, 60 minutes of music | |
| 6 | Lock screen: pause, play, next | works: previous, play/pause and next on the lock screen |
| 7 | Streaming over the VPN | |
| 8 | HTTPS, if the server uses it | |

Run 1 (2026-10-10): only a 15-second MP3 test file in the library; the
collection is not in Jellyfin yet. Tests 3 to 8 wait for it.

Run 2 (2026-10-10, 0.0.1 to 0.0.3, first album of the collection, FLAC):
FLAC streams; with the screen off a track played on without gaps in the
per-minute log. The next track did not start by itself: the player
reports the end of a track before it stops, so a track started on the
end report was stopped again; 0.0.2 starts it once the player has
stopped, which works with the screen off. 0.0.3 adds a position slider,
10-second skips and seeking from the lock screen; seeking in a streamed
FLAC works (the server answers range requests). Reported by the
maintainer and seen in log.txt.

Run 3 (2026-10-10, 0.0.4): after closing the app without networking,
0.0.3 asked for the login first, since it kept nothing between starts.
0.0.4 stores each track's details (name, artist, album, number,
duration, codec; no token, no address) next to the file and lists the
downloads without the server ("Downloaded music"); playback from there
with WLAN off works, reported by the maintainer. The files from before
0.0.4 had no details and were deleted for a new download.
