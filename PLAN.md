# SailTune - Project Plan

Status: 2026-10-10 - name decided (SailTune, held as a draft in Harbour),
playback spike on the Jolla Phone mostly done (section 11, Phase 1), most
decisions made (section 14). Next: where downloads go and the storage
limit, then Phase 0 and Phase 2.

SailTune is the third app after SailVault and SailToken. It shares no
code with them; their build, release and Harbour experience carries over
(section 13).

## 1. Goal

A native, open-source music player for Sailfish OS, published in the
Jolla Store, for the music on the user's own Jellyfin server. It streams
while the server is reachable (at home or over a VPN) and plays albums
and playlists downloaded to the phone without any network. It replaces a
streaming subscription for people who keep their music themselves.

- Package name: `harbour-sailtune` (draft in Harbour since 2026-10-10)
- Primary target: Jolla Phone (2026), aarch64, Sailfish OS 5.2
- Secondary target: armv7hl (Harbour requires it), then i486 (emulator)
- Server: Jellyfin only (decided 2026-10-10; Emby is not targeted)

## 2. Success criteria

| # | Criterion | Measured by |
|---|-----------|-------------|
| 1 | In Harbour | RPM passes the Harbour validator for aarch64 and armv7hl; Jolla QA accepts it |
| 2 | Offline first | Downloaded music plays with no network and no login; the app opens on the library from its cache, also offline |
| 3 | Uninterrupted playback | 60 minutes with the screen off and the phone untouched, from downloads and streamed, without a gap in the playback log; the next track starts by itself |
| 4 | Credentials | The password is never stored; the access token as decided in section 14; logging out ends the session on the server; no secret in any log |
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
| SubMusic (OpenRepos) | MIT, active, Navidrome/Subsonic, Nextcloud, Bandcamp; MPRIS; single-track downloads | No Jellyfin; Store release planned, not confirmed |
| SailStream (Jolla Store) | Plex | Not Jellyfin |

SailTune competes on:

- **Music only**: artists, albums, playlists and the queue, nothing else
- **Offline first**: download albums and playlists, play them anywhere,
  start without a network
- **In the Jolla Store**, not only on OpenRepos
- **Sailfish-native**: Silica, lock screen controls, cover actions
- **Trust**: open source, no telemetry, nothing but the user's server

## 4. Non-goals

- Video, photos, TV, books and podcasts on the Jellyfin server
- Other servers (Emby, Plex, Navidrome/Subsonic) in v1
- Playback or downloads while the app is closed: Harbour allows no
  background service. Downloads run while the app runs, also in the
  background; music plays while the app is open, also with the screen off
- Casting to other devices, party mode, remote control of other
  Jellyfin clients
- Editing the server's library (tags, artwork, deleting files)
- A music store, radio or recommendations from outside the server

## 5. Architecture

```
qml/   Silica UI: library, album, artist, playlists, queue, now playing,
       downloads, settings
src/   C++ (Qt 5.6): Jellyfin client (QtNetwork), library cache (SQLite),
       download manager, player (QtMultimedia), MPRIS bridge data
```

Qt only, no Rust core (section 14). Data that crosses from the server
is parsed in one place in the Jellyfin client, with hard limits.

- **Network**: QNetworkAccessManager for every call and every download,
  with the token in the `Authorization: MediaBrowser ... Token=...`
  header. QtMultimedia sends no custom headers, so streaming passes the
  token as `api_key` in the URL (verified in the spike); such URLs never
  reach a log.
- **Library cache**: SQLite in the app's private storage, filled from
  `/Items` (artists, albums, tracks, playlists) and refreshed on start
  and on demand while online; the UI reads only the cache.
- **Downloads**: original files (`/Audio/{id}/stream?static=true`; a
  quality choice follows in Phase 7), stored where section 14 decides,
  named `<item id>.<extension>`; the cache records which tracks are
  offline.
- **Playback**: one QtMultimedia `Audio` with the music role and a queue
  in C++; a track starts once the player has stopped after the previous
  one (spike, section 11). Amber.Mpris for the lock screen; the cover
  shows the track and has play/pause and next.
- **Reporting**: playback start, progress and stop to `/Sessions/Playing`
  while online; offline plays are queued and sent when the server is
  reachable again (section 14).

Allowed APIs checked 2026-10-10 against the Harbour list: QtMultimedia
5.6, libQt5Multimedia, libQt5Network, libQt5Sql, libsqlite3,
libQt5Concurrent, Amber.Mpris 1.0 (QML), Sailfish.Secrets,
Nemo.Notifications, Nemo.KeepAlive. Sailjail permissions: Audio and
Internet; Music only if downloads go to the public Music folder
(section 14).

## 6. Login and session

- Server address and user name are kept; the password only reaches
  `/Users/AuthenticateByName` and is never stored.
- Jellyfin's Quick Connect (a code shown in the app, approved in another
  Jellyfin client) as a second way to log in, so the password need not
  be typed on the phone (Phase 2, unverified on Jellyfin 12.2).
- One server and one user in v1.
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
- Settings show the space used and the free space; a limit is open
  (section 14).
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
- No product names in the app name, icon or desktop file: no "Jellyfin"
  there; the store text may name the server.
- Versions only digits and dots for a submission (`-c no-fix-version` or
  a clean tag).

## 11. Phases

### Phase 0 - Scaffold

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

### Phase 2 - Login and library

Login (password and Quick Connect), session handling, the SQLite cache
with artists, albums, tracks and playlists, the library pages and
search, cover art cache, limits on every response.

### Phase 3 - Player

Queue, now playing page, MPRIS, cover actions, seeking, resume after a
restart, playback reporting while online.

### Phase 4 - Offline

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

As decided then: gapless playback, a quality choice for downloads,
editing playlists, several servers or users, lyrics, sleep timer, German
translation.

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
  stops.
- Store texts, privacy policy and the AI disclosure belong to the first
  release, not to the last week.

## 14. Open decisions

- **Where downloads go.** Options: the app's private storage (no extra
  permission, invisible to other apps), or the public Music folder
  (permission Music; the system's media player would list them, and
  other apps could read them). Proposal: private storage.
- **Storage limit.** None, a warning, or a hard limit in the settings.
- **Gapless playback.** Whether the short gaps between tracks of a live
  album or a DJ mix matter; QtMultimedia 5.6 cannot avoid them.

Decided:

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
- Playlists (2026-10-10): play and download in v1; creating and editing
  them on the phone is noted for later (Phase 7).

- Name (2026-10-10): SailTune, `harbour-sailtune`, held as a draft in the
  maintainer's Harbour account with a name-check build; it must not be
  submitted for QA. Shanty and Capstan were the other candidates.
- Server (2026-10-10): Jellyfin only; the maintainer moved from Emby.
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
