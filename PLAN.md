# SailTune - Project Plan

Status: 2026-10-10 - name decided (SailTune, held as a draft in Harbour);
v1 serves the phone's music and Jellyfin, OpenSubsonic follows (section
14). Done and verified on the Jolla Phone:

- The phone's music with its album art: browse artists, albums and
  playlists, search, play (Phase 2 part one).
- The player: mini player on every page with 10-second skips, now
  playing page, lock screen controls (MPRIS) with seeking, cover
  actions, and resuming the exact track and position after the app was
  closed (Phase 3 core, pulled ahead of Jellyfin).
- In daily use (2026-10-11): the phone's music played over Bluetooth in
  the maintainer's car during a drive with the screen off, without
  interruption, as the maintainer reported.
- Jellyfin (2026-10-11): adding the maintainer's server with user name
  and password, reading its library, streaming from it (Phase 2 part
  two).
- Downloads (2026-10-11): an album downloaded and played with WLAN off
  (Phase 4 core).
- Offline start (2026-10-11): with the server as the active source and
  WLAN off, the app opens on the server's library from the cache.

Next (Phase 4 rest): a "downloaded only" view for travelling without a
network, keeping the download queue when the app closes, the storage
overview in the settings. Untested so far: Quick Connect.

SailTune is the third app after SailVault and SailToken. It shares no
code with them; their build, release and Harbour experience carries over
(section 13).

## 1. Goal

A native, open-source music player for Sailfish OS, published in the
Jolla Store, for the music on the phone and on the user's own server.
Like S2 on Android, it plays the phone's music from the first start; a
server is a source the user adds. v1 serves Jellyfin;
the code keeps the server family behind one line, so that OpenSubsonic
servers with API keys (such as Navidrome) can follow in a later release
and users choose the server they run. It streams
while the server is reachable (at home or over a VPN) and plays albums
and playlists downloaded to the phone without any network. It replaces a
streaming subscription for people who keep their music themselves.

- Package name: `harbour-sailtune` (draft in Harbour since 2026-10-10)
- Primary target: Jolla Phone (2026), aarch64, Sailfish OS 5.2
- Secondary target: armv7hl (Harbour requires it), then i486 (emulator)
- Servers: Jellyfin in v1, OpenSubsonic next; several can be set up, one
  is active at a time (decided 2026-10-10)

## 2. Success criteria

| # | Criterion | Measured by |
|---|-----------|-------------|
| 1 | In Harbour | RPM passes the Harbour validator for aarch64 and armv7hl; Jolla QA accepts it |
| 2 | Offline first | The phone's music and downloaded music play with no network and no login; the app opens on its library, never on a login page, also offline and on the first start |
| 3 | Uninterrupted playback | 60 minutes with the screen off and the phone untouched, from downloads and streamed, without a gap in the playback log; the next track starts by itself |
| 4 | Credentials | No password is ever stored: Jellyfin keeps its access token, OpenSubsonic its API key, both revocable on the server; logging out ends the session where the server has one; no secret in any log |
| 5 | No broken downloads | A download is written to a temporary file, checked against the server's size and only then renamed; an interrupted download leaves nothing half-written; the library cache can always be rebuilt from the server |
| 6 | Bounded input | Every server response has size and count limits; file names come from item ids, never from server text |
| 7 | Fast daily use | Library visible < 1 s after start from the cache; a downloaded track starts < 0.5 s after the tap |
| 8 | Native UI | Silica components only; lock screen controls (MPRIS) and cover actions |

## 3. Positioning (as of 2026-10)

Music clients for self-hosted servers on Sailfish (desk research
2026-10-10, not tested first-hand):

| App | State | Gap |
|-----|-------|-----|
| Sailfin (OpenRepos) | Jellyfin client for all media, alpha, 0.6.0 (2026-09) | Not in the Jolla Store; offline not mentioned; video-first |
| SubMusic (OpenRepos) | MIT, active, Navidrome/Subsonic, Nextcloud, Bandcamp; MPRIS; single-track downloads | No Jellyfin; whole albums offline not yet; Store release planned, not confirmed |
| SailStream (Jolla Store) | Plex | Not Jellyfin |

SailTune competes on:

- **Built for more than one server family**: Jellyfin in v1, OpenSubsonic
  next, several servers set up, one active at a time
- **Music only**: artists, albums, playlists and the queue, nothing else
- **Offline first**: download albums and playlists, play them anywhere,
  start without a network
- **In the Jolla Store**, not only on OpenRepos
- **Sailfish-native**: Silica, lock screen controls, cover actions
- **Trust**: open source, no telemetry, nothing but the user's server

## 4. Non-goals

- Video, photos, TV, books and podcasts on the Jellyfin server
- Any server family but Jellyfin in v1; OpenSubsonic follows first, Emby
  and Plex may follow one at a time
- A library that mixes several servers at once
- Playback or downloads while the app is closed: Harbour allows no
  background service. Downloads run while the app runs, also in the
  background; music plays while the app is open, also with the screen off
- Casting to other devices, party mode, remote control of other
  Jellyfin clients
- Editing the server's library (tags, artwork, deleting files)
- A music store, radio or recommendations from outside the server

## 5. Architecture

```
qml/   Silica UI: servers, library, album, artist, playlists, queue,
       now playing, downloads, settings
src/   C++ (Qt 5.6): one client per server family behind a common
       interface (QtNetwork), library cache (SQLite), download manager,
       player (QtMultimedia), MPRIS bridge data
```

- **Server families behind one line**: the library, downloads, player and
  UI talk to "the active server": browse, search, stream URL, download
  request, report a play, favorites. `JellyfinServer` implements it in
  v1, `SubsonicServer` after; nothing above that line knows which one it
  talks to. The line is drawn as narrow as Jellyfin needs and checked
  against the OpenSubsonic API on paper, so the second family fits
  without reshaping it.
- **Sources**: the phone's music is always there; servers are added.
  `LocalSource` reads the phone's music, `JellyfinServer` a Jellyfin
  server; both stand behind the same line.
- **One active server**: several servers can be set up; one is active,
  and the library, queue and search belong to it. Switching keeps every
  server's downloads and cache.
- **Neutral identities**: every cached item and download carries the
  server's local id and the item id as text, since Jellyfin and Subsonic
  ids look different; downloads live in `downloads/<server id>/`.

Qt only, no Rust core (section 14). Data that crosses from the server
is parsed in one place in the Jellyfin client, with hard limits.

- **Network**: QNetworkAccessManager for every call and every download.
  Jellyfin takes the token in the `Authorization: MediaBrowser ...
  Token=...` header; Subsonic takes its credentials as URL parameters on
  every call. QtMultimedia sends no custom headers, so a stream URL
  carries the credentials in both cases (`api_key` for Jellyfin, verified
  in the spike); such URLs never reach a log.
- **Library cache**: SQLite in the app's private storage, filled from
  Jellyfin's `/Items` or Subsonic's `getArtists`, `getAlbum` and
  `getPlaylists`, refreshed on start and on demand while online; the UI
  reads only the cache.
- **Downloads**: original files (Jellyfin `/Audio/{id}/stream?static=true`,
  Subsonic `download`; a quality choice follows in Phase 7) in
  `downloads/<server id>/` in the app's private storage, named
  `<item id>.<extension>`; the cache records which tracks are offline.
- **Playback**: one QtMultimedia `Audio` with the music role and a queue
  in C++; a track starts once the player has stopped after the previous
  one (spike, section 11). Amber.Mpris for the lock screen; the cover
  shows the track and has play/pause and next.
- **Reporting**: Jellyfin's `/Sessions/Playing` (start, progress, stop),
  Subsonic's `scrobble`; offline plays are queued and sent when the
  server is reachable again (section 14).

Allowed APIs checked 2026-10-10 against the Harbour list: QtMultimedia
5.6, libQt5Multimedia, libQt5Network, libQt5Sql, libsqlite3,
libQt5Concurrent, Amber.Mpris 1.0 (QML), Sailfish.Secrets,
Nemo.Notifications, Nemo.KeepAlive. Sailjail permissions: Audio and
Internet; Music, to play the phone's music and read its album art;
MediaIndexing, to read the phone's music from the system's media index.

## 6. Login and session

- No start ever asks for a server, the first one included (maintainer,
  2026-10-10). The app opens on its library: the phone's music, or the
  active server's library from the cache, online or not. Without a
  server and without music on the phone it shows a hint to add one.
- Adding a server is an action in the pulley menu and the settings: the
  user types the server's address, which people who run a server know.
  No discovery in v1 (section 14).
- A token the server no longer accepts shows a "log in again" note on
  the library; it blocks nothing, and the downloads keep playing.

- Adding a server: its address; the app asks the server what it is
  (Jellyfin's `/System/Info/Public`, Subsonic's `ping`) instead of
  asking the user.
- Jellyfin: server address and user name are kept; the password only
  reaches `/Users/AuthenticateByName` and is never stored.
- OpenSubsonic: only servers with the `apiKey` extension; the user
  creates a key on the server and enters it. A server with only the
  classic login is refused with an explanation (section 14).
- Jellyfin's Quick Connect (a code shown in the app, approved in another
  Jellyfin client) as a second way to log in, so the password need not
  be typed on the phone (Phase 2, unverified on Jellyfin 12.2).
- Several servers, one active; one user per server.
- Logging out calls `/Sessions/Logout`, deletes the token, and asks
  whether to keep the downloads.
- HTTPS is recommended; plain HTTP is allowed for a server reached only
  at home or through a VPN, with a note in the login page. A self-signed
  certificate is accepted only after the user confirms its fingerprint,
  as in SailToken's sync, for the calls of the Jellyfin client.
  QtMultimedia's own TLS cannot be pinned, so v1 streams only over HTTP
  inside a VPN or HTTPS with a certificate the phone trusts (section
  14).

## 7. Offline and downloads

- Albums and playlists are made available offline as a whole; single
  tracks follow from the queue's menu.
- The download manager runs one download at a time, survives leaving the
  page, and resumes or restarts after the app was closed (Range
  requests; Jellyfin answered range requests for seeking in the spike).
- Downloads stop before the phone's free space falls below a reserve;
  settings show the space used, the largest albums and "remove all
  downloads" (section 14).
- Without a network the app switches to the downloads by itself and says
  so; streaming needs no extra mode.
- Removing an album from the phone deletes its files and its offline
  flag, never anything on the server.

## 8. Playback

- Queue: play an album, a playlist or the whole artist from any track;
  add to the queue, play next, shuffle, repeat.
- Position, queue and the current track survive closing the app.
- Seeking by slider and 10-second steps, also from the lock screen
  (spike, section 11).
- Gapless playback: QtMultimedia 5.6 has none; whether the gaps matter
  and what could close them is open (section 14).
- Audio focus: a call pauses the music (Sailfish resource policy,
  unverified for a Harbour app).

## 9. Library

- Pages: artists, albums, album, artist, playlists, playlist, search,
  downloads, queue, now playing.
- Cover art from `/Items/{id}/Images/Primary` at the size shown, cached
  on disk with a size limit.
- Favorites and play counts from the server; marking a favorite while
  offline is sent later.

## 10. Harbour constraints

- Only libraries, QML imports and Sailjail permissions from the
  allowlist; checked before use (section 5).
- No daemons, no background service, no autostart.
- Installed files under `/usr/share/harbour-sailtune` except binary,
  desktop file and icons.
- No product names in the app name, icon or desktop file: no
  "Jellyfin", "Subsonic" or "Navidrome" there; the store text may name
  the servers.
- Versions only digits and dots for a submission (`-c no-fix-version` or
  a clean tag).

## 11. Phases

### Phase 0 - Scaffold (done 2026-10-10)

Repository, `.gitignore` with the agent files, MIT license, README with
the AI disclosure, `harbour-sailtune.pro`, spec, desktop file with Audio
and Internet, a first icon, build for aarch64 and armv7hl, validator
clean. The name-check build in `name-test/` is the starting point.

### Phase 1 - Playback spike (mostly done)

`spikes/playback/` (harbour-musicspike), results in its README, on the
Jolla Phone with Sailfish OS 5.2.0.18 and Jellyfin 12.2.0, 2026-10-10:

- Login, albums and tracks; MP3 and FLAC stream and play.
- Download of a 27-track FLAC album (900 MB) at about 40 MB/s at home.
- After closing the app with networking off, the downloads play without
  login (0.0.4 keeps each track's details next to the file).
- Lock screen: previous, play/pause, next; seeking by slider and 10 s
  steps, also in a streamed FLAC.
- The next track did not start by itself at first: the player reports
  the end of a track before it stops; starting the next one on the stop
  works, also with the screen off.
- Several minutes with the screen off without a gap.

Open: a long run with the screen off (the maintainer will use the spike
day to day instead of a dedicated hour), streaming over the VPN, AAC and
Opus files, HTTPS.

### Phase 2 - Sources and library

Part one (the phone, done 2026-10-10): the source line, the SQLite
cache, the phone's music from the media index, the library pages and
search.

Part two (Jellyfin, next):

1. Sources page, "Add server": the server's address, checked with
   `/System/Info/Public`; then user name and password, or Quick
   Connect. The password is never stored, only the access token, in an
   owner-only file in the source's folder.
2. Read the server's library (artists, albums, tracks, playlists) page
   by page with size limits into the cache, with cover art from the
   server, so the library also opens without a network.
3. Stream through the same player (`/Audio/{id}/stream?static=true`
   with `api_key`, never logged).
4. A token the server no longer accepts shows only a "log in again"
   note; nothing is blocked, downloads keep playing.

What part one and two cover together, as first planned:

The source spike (`spikes/sources/`, done) showed the system's media
index works. Then the source line with `LocalSource` and `JellyfinServer`, adding and switching servers,
login (password and Quick Connect), the SQLite cache with artists,
albums, tracks and playlists, the library pages and search, cover art
cache, limits on every response. Tested against the maintainer's
Jellyfin.

### Phase 3 - Player (pulled before Jellyfin, core done 2026-10-10)

Pulled ahead on 2026-10-10 so the phone's music is playable at once;
done and verified on the device: queue, now playing page, mini player
with 10-second skips, MPRIS with seeking, cover actions, resume of the
exact position after a restart. Open: playback reporting, which needs
a server.


Queue, now playing page, MPRIS, cover actions, seeking, resume after a
restart, playback reporting while online.

### Phase 4 - Offline (core done 2026-10-11)

Download manager, offline albums and playlists, start without network,
storage overview and removal, the offline switch-over.

### Phase 5 - Playlists and favorites

Playlists from the server (play and download; editing later),
favorites, queued reporting of offline plays.

### Phase 6 - Review and Harbour submission

Security review (token handling, input limits, file names), threat
model, store texts, privacy policy, screenshots, release from a clean
tag, submission into the existing draft.

### Phase 7 - After the first release

First OpenSubsonic (`SubsonicServer`, API keys only; a spike against a
Navidrome in Docker on the development PC checks which servers qualify).
Then as decided: gapless playback, a quality choice for downloads,
editing playlists, Emby, lyrics, sleep timer, German translation.

## 12. Security principles

- The password is never stored or logged; the token is stored only as
  decided in section 14 and never logged, also not inside a URL.
- Nothing is sent anywhere but the user's server; no telemetry, no crash
  reports.
- Server responses are untrusted: size limits, count limits, strict JSON
  parsing; ids, not names, become file names.
- Downloads are written atomically and checked; nothing executable is
  ever stored.
- The release build writes no log file; the spike's log is a test tool.

## 13. Lessons from SailVault and SailToken, applied from the start

- Check the Harbour package name before anything carries it: done
  (2026-10-10, held as a draft; SailToken had to be renamed after a
  refusal).
- Spike the risky platform parts on the device first; claim nothing that
  was not measured there.
- Validator clean from the first build, both architectures, release
  builds from a clean tag.
- Qt 5.6 APIs only; QtMultimedia reports the end of a track before it
  stops, so the next track starts on the stop (spike, 2026-10-10).
- Sailfish stops the media player when the app closes, and a stopped
  player reports position 0; swiping the app away may also skip
  `aboutToQuit`. The position to resume is therefore the last one while
  playing or paused, saved while playing, on pause and when the app goes
  to the background (seen on the device, 2026-10-10).
- Store texts, privacy policy and the AI disclosure belong to the first
  release, not to the last week.

## 14. Open decisions


- **Gapless playback.** Whether the short gaps between tracks of a live
  album or a DJ mix matter; QtMultimedia 5.6 cannot avoid them.

Decided:

- Reading the phone's music (2026-10-10): from the system's media index,
  no own tag reader. The source spike read 27 songs with title, artist,
  album, track number and duration in 81 ms through TinySPARQL's
  `Serialize` (JSON-LD over D-Bus, permission `MediaIndexing`), and the
  system's album art in `~/.cache/media-art` (permission `Music`).
- No server discovery in v1 (2026-10-10): the user types the address.
  Jellyfin's UDP discovery got no answer from the maintainer's server, a
  container app on TrueNAS that publishes no UDP port 7359 and would name
  its container address; people who run a server know its address.

- The phone's music (2026-10-10): v1 plays the music on the phone, as
  S2 on Android does, so the app is useful from the first start without
  a server; servers are added as sources. Needs the `Music` permission,
  and how tags are read is open (above). A leaner v1 without it was
  weighed.
- Testing (2026-10-10): Phase 2 is tested on the device against the
  maintainer's own Jellyfin, not against a test server in Docker.

- OpenSubsonic login (2026-10-10): only servers that offer API keys (the
  OpenSubsonic `apiKey` extension). The classic login needs the password
  or a salted MD5 token made from it on every call, so the app would
  have to keep something worth as much as the password; the maintainer
  chose no security compromise, as for SailVault and SailToken. Which
  servers offer API keys is checked in the OpenSubsonic spike; the
  servers named in section 1 count only where they do.

- Qt only (2026-10-10): C++ and QML, no Rust core. SailTune keeps no key
  material and needs no crypto; QtNetwork, QtSql and QJsonDocument cover
  the work, and cargo 1.75 with vendoring would be a cost without a
  matching gain.
- Token (2026-10-10): stored in a file readable only by the owner in the
  app's private storage, which Sailjail keeps from other apps; not in
  Sailfish Secrets, whose API Harbour allows without a compatibility
  promise. A Jellyfin token can be revoked on the server, unlike a
  password, which is never stored.
- Downloads (2026-10-10): the original files in v1; a quality choice per
  download (Jellyfin transcoding, for example to Opus or AAC) in Phase 7.
- HTTPS (2026-10-10): v1 streams over plain HTTP inside the VPN or over
  HTTPS with a certificate the phone trusts. QtMultimedia's GStreamer
  cannot pin a self-signed certificate (Sailfin's developer reports the
  same); streaming through a local progressive download by QtNetwork,
  which can pin, stays a later option.
- Playback reporting (2026-10-10): plays go to the server (play counts,
  "recently played", resume); offline plays are queued and sent when the
  server is reachable again.
- Download location (2026-10-10): the app's private storage, named by
  item id. No extra permission, no files changed behind the app's back,
  no doubles in the system's media player. The public Music folder was
  weighed (other apps and the PC over USB could use the files) and left
  for a later "export to the Music folder".
- Storage (2026-10-10): no fixed limit by default, as in the common
  streaming apps; the user chooses what goes offline. Downloads stop
  with a clear message before the phone's free space falls below a
  reserve (2 GB as a start). Settings show the space SailTune uses, the
  largest albums and "remove all downloads". A user-set limit can follow.
- Playlists (2026-10-10): play and download in v1; creating and editing
  them on the phone is noted for later (Phase 7).

- Name (2026-10-10): SailTune, `harbour-sailtune`, held as a draft in the
  maintainer's Harbour account with a name-check build; it must not be
  submitted for QA. Shanty and Capstan were the other candidates.
- Servers (2026-10-10): v1 supports Jellyfin alone, to ship sooner, with
  the server family behind one line in the code; OpenSubsonic follows
  first after v1, so users can then choose their server. OpenSubsonic was
  briefly planned for v1 the same day, since one implementation reaches
  Navidrome and others. Several servers can be set up, one is active at
  a time; a mixed library was weighed and left out. Emby may follow.
- Store first (2026-10-10, as for all the maintainer's apps): the Jolla
  Store is the main channel; OpenRepos at most later.
- License: MIT.

## 15. References

- Jellyfin API: https://api.jellyfin.org/
- Harbour allowed APIs: https://docs.sailfishos.org/Develop/Apps/Harbour/Allowed_APIs/
- Harbour allowed permissions: https://docs.sailfishos.org/Develop/Apps/Harbour/Allowed_Permissions/
- Sailfin: https://openrepos.net/node/11159
- SubMusic: https://openrepos.net/content/jakub/submusic
- Spike results: `spikes/playback/README.md`
