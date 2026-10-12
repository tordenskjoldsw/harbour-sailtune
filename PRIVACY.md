# Privacy policy

SailTune is a music player for Sailfish OS that plays the music on the
phone and on Jellyfin servers you add. This policy describes what the
app does with your data. It applies to SailTune 0.5 and later.

## What SailTune collects

Nothing. SailTune has no analytics, no crash reporting, no advertising
and no account. It never sends data to the developer or to any third
party.

## Music on your phone

SailTune reads the list of music files and their album art from the
phone's media index, on the phone. It never changes, moves or uploads
these files.

## Servers you add

SailTune connects only to the Jellyfin servers you add, at the address
you type. For each server it sends and stores:

- To log in, your user name and password, or a Quick Connect code you
  approve in another Jellyfin app. The password is sent once and never
  stored.
- In every request, the name "SailTune", the device name "Sailfish OS",
  the app version and a random device id SailTune created for this
  server. The server uses them to list your sessions and devices.
- What you play and when, and the tracks you mark as favorites, so that
  your server shows them in your history and favorites. While the
  server cannot be reached, these wait on the phone and are sent later.
- Tracks you add to a playlist and playlists you create.

The server's operator can see these requests, as for any Jellyfin app.
If someone else runs the server, their privacy policy applies to the
data on it.

Over plain http, which SailTune allows only with a warning, the
password, the access token and your music travel unencrypted; use it
only at home or through a VPN.

## Where your data is stored

All in SailTune's private app directory, which other sandboxed apps
cannot read:

- For each server: its address, your user name and user id, the device
  id, and the access token, in a file only SailTune can read.
- A copy of each library (artists, albums, tracks, playlists,
  favorites), so the app opens quickly and works offline.
- Downloaded tracks and their covers, and plays and favorites that wait
  to be sent. If you choose the memory card for downloads in the
  settings, the tracks downloaded from then on are saved there instead,
  in the hidden folder `.sailtune`; the card has no file permissions,
  so other apps allowed to use the memory card can read them.
- The settings, the playing queue and the position in the current track.

Removing a server logs it out and deletes all of the above for it,
including its downloads, also those on a memory card that is in the
phone. Uninstalling SailTune deletes everything in its app directory;
downloads on a memory card stay until you delete the `.sailtune`
folder.

## Permissions

- **Audio:** to play music and take the media keys and lock screen
  controls.
- **Music and MediaIndexing:** to find the music on the phone and its
  album art.
- **Internet:** only for the servers you add.
- **RemovableMedia:** to save downloads on the memory card, only if
  you choose it in the settings.

## Contact

Questions about privacy: open an issue at
https://github.com/tordenskjoldsw/harbour-sailtune/issues.
