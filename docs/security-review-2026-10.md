# Security review, October 2026

Reviewed state: commit `9149aaa` and its parents up to `106aaef`
(Phase 6, before the first store release 0.5.0), 2026-10-11. Scope:
`src/` (sources, Jellyfin client, downloads, player, cache), `qml/`, the
spec, the desktop file and the `.pro` file, against `PLAN.md` sections
6, 7, 10 and 12.

Method: one review of the whole tree for token and password handling,
file names from untrusted ids, limits on untrusted input, TLS and
redirects, SQL, Harbour rules and object lifetimes. Every finding below
was re-checked against the code before it was fixed. Device results come
from the Jolla Phone (Sailfish OS 5.2.0.18) against the maintainer's
Jellyfin 12.2.0.

Result: no high findings; two medium and seven low findings, all fixed.

## Findings

**M1: an empty or redirected answer counted as success.** Qt 5.6 does
not follow redirects, so a captive portal's or a proxy's answer without
a body reached the refresh as an empty library, which emptied the
cache, deleted the kept covers and dropped the queued plays and
favorites. Fixed in `c3fa271`: redirects and gateway errors count as
unreachable, a library page needs `Items` and `TotalRecordCount`, and a
queued call is done only when Jellyfin answers with the user data.

**M2: plain HTTP without the note the plan requires.** An address
without a scheme became http, and the login page did not say that the
password and the token then travel unencrypted. Fixed in `b7dbc2f` (the
note) and `b1619bf`, which tries https first for an address without a
scheme.

**L1: removing a server during its download** left the download with a
null source. Fixed in `97c1072`.

**L2: the logout on removing a server was not sent**, since its network
manager was deleted with the source; the token stayed valid on the
server. Fixed in `c3fa271`.

**L3: QUuid accepts an id with trailing text**, so a changed
`servers.json` could make removing a server delete a folder outside
`sources/` (needs write access to the app's private configuration).
Fixed in `c3fa271`: ids must match the UUID form exactly.

**L4: a non-music answer was stored as a track**, as a captive portal's
page, and error answers were read into memory unbounded. Fixed in
`97c1072`.

**L5: titles were rendered as rich text.** A title from a file's tags or
a server such as `<img src="http://...">` made the app load a remote
image. Fixed in `574dd0c`: plain text everywhere, and for PageHeader a
zero-width space after `<`.

**L6: missing limits** on the token, the user id, the Quick Connect
secret and its flag answer, and on playlist entries over all playlists.
Fixed in `c3fa271`.

**L7: the phone index worker called back through a raw pointer**, which
could be gone when the app quit during a read. Fixed in `20d798b`.

## Checked and clean

- No logging in `src/` or `qml/`.
- The token is stored only in `token` (0600, in a 0700 folder) and sent
  only to the server it belongs to: as a header for the library and
  downloads, as `api_key` in the stream URL, which only QtMultimedia
  gets. The password is sent once and never stored.
- File names: downloads only from `[A-Za-z0-9_-]{1,128}`, covers only
  from ASCII letters, digits and `-`, queued ids checked on load.
- TLS errors are never ignored; no redirect is followed with the token.
- SQL: every value is bound.
- Limits: JSON answers 32 MiB, covers 5 MiB, tracks 2 GiB with a 2 GiB
  free-space reserve, `servers.json` 1 MiB and 32 servers,
  `pending.json` 2 MiB and 5000 entries, the download queue 4 MiB, the
  media index answer 64 MiB, Quick Connect 100 polls.
- Harbour: QML imports QtQuick 2.0, Sailfish.Silica 1.0, Amber.Mpris 1.0
  and the app's own module; files only in the allowed places; the four
  permissions are used.

## Open, by decision

- Self-signed certificates are not offered for confirmation in v1, as
  streaming could not use them (`PLAN.md` section 14).

Also changed after the review: the player showed QtMultimedia's own
error text, which might name the stream URL with the token (whether
GStreamer does is unverified); it now shows SailTune's own message.
