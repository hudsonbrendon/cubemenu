# Changelog

## 0.1.4 - 2026-10-17

- Launch scene: the GameCube rebuilt from photographs of the console. Indigo shell with softened
  edges and base seam, the disc lid with notches round the corner buttons and the round black badge,
  POWER / RESET in Indigo and OPEN in light grey, the power light, the light-grey front panel with
  four round controller ports and their dots and two memory card doors, 8 x 8 vent grilles with
  their lip on both sides, the black carrying handle, and inside the bay the black turntable, chrome
  spindle, laser sled and the label under the lid. Plastic is lit with a specular highlight. The
  camera starts three-quarter, rises over the top while the disc goes in and comes back down to
  the badge. Always Indigo, whatever the theme.
- Start-up trace on screen and in `/cubemenu/boot.log` (0.1.3), lighter: one line per step.

## 0.1.2 - 2026-10-17

- Install through Swiss: GC Loader keeps booting Swiss's own `boot.iso`, and `cube-card install`
  sets Swiss's *Load at startup* to `cubemenu.dol`. Cube Menu's `boot.iso` still froze on the
  GameCube logo on a real console after 0.1.1, so it is now opt-in (`install --mode iso`) and
  experimental; its header now fills the user area fields that real discs carry.
- "Open Swiss" and holding B at power on pass an empty `Autoload=`, so Swiss does not bounce back
  to the menu.

## 0.1.1 - 2026-10-17

- boot.iso: fix the console freezing on the GameCube logo. The apploader wrote stale data-cache lines
  back over the menu the drive had just loaded (Dolphin does not emulate that cache, so it only showed
  on hardware); destinations are now invalidated, and all its state lives inside the loaded image.

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
