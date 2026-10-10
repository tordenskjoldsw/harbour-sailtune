# Spike: the phone's music and server discovery

Questions: can a Jolla Store app read the music on the phone from the
system's media index, use the album art the system extracted, and find
Jellyfin servers in the home network?

Harbour allows no tag library (no TagLib, no GStreamer libraries, no
Tracker QML module), but the `MediaIndexing` permission lets an app talk
to `org.freedesktop.Tracker3.Miner.Files` over D-Bus, and `Music` opens
`~/Music`, `~/Playlists`, the Android music folders and
`~/.cache/media-art` (read from `/etc/sailjail/permissions/` on the
phone). The phone runs TinySPARQL 3.9.2, whose endpoint offers
`Serialize(query, fd, flags, format, args)`; format 2 is JSON-LD, which
Qt reads without any extra library.

The app sends a SPARQL `CONSTRUCT` for every `nmm:MusicPiece` with its
title, duration, track number, file URL, album title and artist name,
reads the answer from a pipe on a worker thread (32 MB cap, 20 s
timeout) and joins the JSON-LD nodes. It counts the files in
`~/.cache/media-art`, plays a song by its file URL, and sends Jellyfin's
discovery broadcast ("Who is JellyfinServer?", UDP port 7359).

Sailjail permissions: Audio, Internet, Music, MediaIndexing.

## Validator

`sfdk check` (SailfishOS-5.1.0.11-aarch64, 2026-10-10): the Music and
MediaIndexing permissions, QtDBus and QtConcurrent pass; the missing icon
is the only failure.

## Results (2026-10-10, Jolla Phone, Sailfish OS 5.2.0.18)

27 FLAC files (one album) copied into `~/Music/SailTune-Test/`:

| Test | Result |
|------|--------|
| Media index, JSON-LD | 27 songs with title, artist, album, track number and duration; 15 KB in 81 ms (D-Bus reply after 57 ms) |
| Media index, Turtle | 23 KB; not used, JSON-LD is parsed |
| Album art | 3 files `album-<hash>-<hash>.jpeg` in `~/.cache/media-art`, readable |
| Discovery | no answer; the same broadcast from the development PC got none either, so the server does not answer (discovery off or UDP 7359 not published), not proven blocked on the phone |

Also seen: an app's data directory stays after the app is uninstalled
(the playback spike's downloads were still there).
