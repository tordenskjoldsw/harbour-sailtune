# Jolla Store listing

Settings for the Harbour submission, kept here for later updates. The
texts to paste into the form are in `docs/store/`, one paragraph or list
item per line, so the form keeps them intact. Limits are the store's.

## Title (max. 30, best under 25)

SailTune

## Summary (max. 200)

`docs/store/summary.txt`

## Description (max. 4000)

`docs/store/description.txt`

## Recent changes (max. 2000)

`docs/store/recent-changes.txt`

## Category

Music, or the closest the form offers.

## Binaries

Harbour asks for at least aarch64 and armv7hl, both built from the
release tag with `-c no-fix-version` and checked with `sfdk check`:

- `harbour-sailtune-<version>-1.aarch64.rpm`:
  `sfdk -c no-fix-version -c target=SailfishOS-5.1.0.11-aarch64 build`
- `harbour-sailtune-<version>-1.armv7hl.rpm`:
  `sfdk -c no-fix-version -c target=SailfishOS-5.1.0.11-armv7hl build`

The build is in the source tree, so remove the objects, `moc_*`,
`Makefile` and the binary in the top folder before each release build:
`make` does not rebuild `main.o` when only the version changes, and the
other architecture's objects do not link. The armv7hl build is not tested
on a device.

## Compatibility

Phone. The app supports both orientations.

## Visual assets

- Icon: `icons/172x172/harbour-sailtune.png`
- Screenshots: at least 1080 px wide, taken on the Jolla Phone (1032 px
  wide, volume up and down together) and scaled to 1080 px with
  Lanczos. They show Jellyfin's public demo server rather than the
  maintainer's library, and only the covers of Binärpilot's "You Can't
  Stop Da Funk" (Candy Mind Records, 2006, CC BY-NC-ND 2.5, archive.org
  item CANDY019, MusicBrainz) and "Promo" (µHz, 2004, CC BY-NC-ND 2.0,
  archive.org item uhzcdr002); the description credits both. Leap
  Fidei's "Nemesis" (2024) has no free license found, so no screenshot
  shows it, neither the album list nor the mini player.
- Cover image (optional): 1080x540.

## Contact details

- Website: https://github.com/tordenskjoldsw/harbour-sailtune
- Privacy policy:
  https://github.com/tordenskjoldsw/harbour-sailtune/blob/main/PRIVACY.md
- Open source project URL: https://github.com/tordenskjoldsw/harbour-sailtune
- Email: the maintainer's contact address for Jolla (not stored here)

## Publish settings

Immediately after QA.

## Message to QA

`docs/store/qa-message.txt`
