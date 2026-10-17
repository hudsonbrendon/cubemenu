# Changelog

## 0.1.0 - 2026-10-17

First release.

- 3D shelf of GameCube cases (front, spine and back art) with floor reflections and the mini disc
  sliding out of the selected case; grid and list views.
- Background gameplay clips (CMPR frames streamed from the card) and per-game theme music.
- Details screen: mini video, screenshots, spinning disc, developer / publisher / year / genre /
  players, synopsis in Portuguese, English and Spanish.
- Launch scene: the console opens its lid, takes the disc, closes and powers on; then Swiss boots the
  game through `Autoload=` in its arguments.
- Themes in the console colours (Indigo, Jet Black, Platinum, Spice, Emerald Blue, Starlight Gold),
  buttons in the controller colours.
- Favourites, recently played, sort orders, jump by letter, multi-disc picker, play time across reboots,
  System screen, screensaver, clock adjustment, settings saved to the card.
- Memory cards screen (slot A / B): saves with icon, comment, blocks and date.
- `boot.iso` for GC Loader with its own apploader; `cubemenu.dol` for other loaders.
- `cube-card` tool: scan, fetch (GameTDB, libretro thumbnails, Wikipedia, yt-dlp), build, install.
