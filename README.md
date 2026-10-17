<p align="center"><img src="assets/brand/logo.png" width="300" alt="Cube Menu"></p>

<p align="center"><img src="docs/screenshots/demo.gif" width="560" alt="Cube Menu: the 3D shelf with gameplay clips and the launch scene"></p>

<p align="center"><b>A good-looking, open game menu for the Nintendo GameCube. Games start through Swiss.</b></p>

<p align="center">
<a href="https://github.com/hudsonbrendon/cubemenu/actions/workflows/build.yml"><img src="https://github.com/hudsonbrendon/cubemenu/actions/workflows/build.yml/badge.svg" alt="build"></a>
<a href="https://github.com/hudsonbrendon/cubemenu/releases"><img src="https://img.shields.io/github/v/release/hudsonbrendon/cubemenu" alt="release"></a>
<a href="LICENSE"><img src="https://img.shields.io/badge/license-MIT-green" alt="MIT"></a>
<a href="https://buymeacoffee.com/hudsonbrendon"><img src="https://img.shields.io/badge/Buy%20me%20a%20coffee-%E2%98%95-FFDD00?logo=buymeacoffee&logoColor=black" alt="Buy me a coffee"></a>
</p>

Cube Menu is what your GameCube shows first: a 3D shelf of game cases with the mini disc sliding out
of the selected one, a gameplay clip running behind it and the game's theme playing. Press A and the
console appears on screen, opens its lid, takes the disc and closes; then the game boots through
[Swiss](https://github.com/emukidid/swiss-gc), which keeps doing what it does best (device drivers,
patches, cheats, video modes). It is the sibling of [Cast Menu](https://github.com/hudsonbrendon/castmenu)
for the Dreamcast.

## Features

- **3D shelf**: GameCube keep cases with the full cover art (front, spine and back), reflections on the floor,
  and the 8 cm disc sliding out of the case you are on. Also a **grid** and a **list** view.
- **Per game**: background gameplay clip, theme music, synopsis (Portuguese, English, Spanish),
  screenshots, developer / publisher / year / genre / players, play time and last played.
- **Details screen**: mini video, screenshots, the disc spinning, the synopsis to scroll through.
- **Launch scene**: the console (in your theme's colour) opens its lid, the game's disc floats down
  onto the spindle, the lid closes, the power light comes on, then the game starts.
- **Themes in the console colours**: Indigo (default), Jet Black, Platinum, Spice, Emerald Blue and
  Starlight Gold. Buttons are drawn in the controller's colours: A green, B red, X / Y grey, Z purple.
- **Library**: categories (all, favourites, recently played), sort by name / recent / most played / year,
  jump by letter with the C-stick, multi-disc games ask which disc to boot.
- **Play time** per game, measured across reboots, and a System screen with the most played games.
- **Memory cards**: the saves in slot A and B with their icons, comments, blocks and dates, next to the
  game's cover (read-only; Swiss's file manager copies and deletes).
- **Settings** saved to the card: language, view, background video, game and menu music, UI sounds,
  volume, 24-hour clock, clock adjustment, screensaver, launch animation.
- **Card tool** (`cube-card`): scans `/games`, downloads covers, disc art, screenshots, synopses,
  clips and music, converts everything to native GameCube texture formats, installs the menu.

## Screenshots

https://github.com/hudsonbrendon/cubemenu/releases/download/v0.1.0/cubemenu-demo.mp4

| | |
|---|---|
| ![shelf](docs/screenshots/coverflow.png) | ![details](docs/screenshots/details.png) |
| ![shelf](docs/screenshots/coverflow-2.png) | ![details](docs/screenshots/details-2.png) |
| ![launch](docs/screenshots/launch-2.png) | ![launch](docs/screenshots/launch-3.png) |
| ![grid](docs/screenshots/grid.png) | ![list](docs/screenshots/list.png) |
| ![settings](docs/screenshots/settings.png) | ![system](docs/screenshots/system.png) |
| ![Spice](docs/screenshots/theme-spice.png) | ![Platinum](docs/screenshots/theme-platinum.png) |
| ![Jet Black](docs/screenshots/theme-jet-black.png) | ![Emerald Blue](docs/screenshots/theme-emerald.png) |

Captured in the Dolphin emulator from a real card's library (play times on the System screen are sample data).

## Quick start (GC Loader)

You need a GameCube with a **GC Loader** (or another device Swiss boots from, see below), an SD card with
your games laid out the way Swiss likes them, and Swiss's `boot.dol` on the card.

```
SD card
├── boot.iso              <- Swiss (GC Loader boots it; Swiss starts Cube Menu)
├── games/
│   ├── F-Zero GX/game.iso
│   ├── Resident Evil 4/disc1.iso
│   ├── Resident Evil 4/disc2.iso
│   └── ...
├── swiss/boot.dol        <- Swiss, started by Cube Menu with the chosen game
└── cubemenu/             <- written by the card tool (art, clips, music, settings)
```

1. Download `cubemenu-<version>.zip` from [Releases](https://github.com/hudsonbrendon/cubemenu/releases)
   and unzip it. It has `boot.iso`, `cubemenu.dol`, the card tool and the docs.
2. On the computer: Python 3 with Pillow and numpy; `ffmpeg` and `yt-dlp` for clips and music.
3. With the SD card mounted (here `/Volumes/GAMECUBE`), from the unzipped folder:

   ```bash
   python3 -m tools.cube_card scan    /Volumes/GAMECUBE    # what is on the card
   python3 -m tools.cube_card fetch   /Volumes/GAMECUBE    # covers, discs, screenshots, synopses, clips, music
   python3 -m tools.cube_card build   /Volumes/GAMECUBE    # convert for the console
   python3 -m tools.cube_card install /Volumes/GAMECUBE --dol cubemenu.dol
   ```

   `install` copies the menu to `/cubemenu/cubemenu.dol` and turns on Swiss's **Load at startup**
   (`Autoload=` in `swiss/settings/global.ini`, the old file is kept as `global.ini.cubemenu-bak`), so
   GC Loader keeps booting your Swiss `boot.iso` and Swiss opens Cube Menu. To stop it, set
   *Load at startup* back to none in Swiss's settings.
4. Eject the card, turn the GameCube on.

**Other devices.** `cubemenu.dol` reads the card from GC Loader, SD2SP2 or an SD Gecko (slot B or A),
in that order, and hands Swiss the same path prefix Swiss uses (`gcldr:/`, `sdc:/`, `sdb:/`, `sda:/`).
On an SD2SP2 / PicoBoot setup, boot `cubemenu.dol` the way you boot other DOLs (for example as the
IPL replacement's first program) and keep Swiss at `/swiss/boot.dol`.

## Using the menu

| Screen | Buttons |
|---|---|
| Shelf / grid / list | ◀ ▶ (▲ ▼) move · **A** play · **Y** details · **X** settings · **B** recently played · **START** system · **L / R** category · **Z** favourite · **C-stick** jump by letter |
| Details | **L / R** or ◀ ▶ previous / next game · ▲ ▼ scroll · **A** play · **Y** favourite · **B** back |
| Settings | ▲ ▼ · ◀ ▶ / **A** change · **B** save and close |
| System | **A** open Swiss (no game) · **Y** memory cards · **B** back |
| Memory cards | **L / R** slot A / B · ▲ ▼ · **B** back |

Hold **B** while turning the console on to skip the menu and go straight to Swiss. **Settings → Open Swiss** (or **A** on the
System screen) starts Swiss on its own, for everything Cube Menu does not do (game settings, cheats,
file management, other devices).

## How it works

- **Boot**: GC Loader boots Swiss as usual and Swiss's *Load at startup* opens `cubemenu.dol`. (The
  release also has an experimental `boot.iso` with its own apploader; it does not boot on a real
  console yet, see the changelog.)
- **Library**: the menu scans `/games/*/` for `.iso` / `.gcm` and reads each header (game ID, disc number).
  Art and metadata come from `/cubemenu/games/<GAMEID>/`, written by the card tool.
- **Launching**: the menu loads Swiss's `boot.dol` and starts it with one argument,
  `Autoload=gcldr:/games/<Name>/game.iso`. Swiss reads its arguments as settings, so it boots that game
  with your per-game settings, cheats and patches, exactly as if you had picked it in Swiss. Swiss is not
  modified; Cube Menu only uses that argument.
- **Clips** are pre-converted on the computer into CMPR (the GameCube's S3TC texture format) frames, so the
  console streams them straight into texture memory with no decoding. Covers, discs and screenshots are
  GX textures too. Music is MP3, decoded with libmad.
- **Play time**: the launch time is saved to the card; the next time the menu starts, the time since then
  is credited to that game (sessions over 12 hours are ignored).

Details in [docs/HOW_IT_WORKS.md](docs/HOW_IT_WORKS.md). Card layout and the card tool in
[docs/CARD.md](docs/CARD.md). Building and testing in Dolphin in [docs/BUILD.md](docs/BUILD.md).

## Support the project

Cube Menu is free and open source. If it made your GameCube nicer to turn on, you can
[buy me a coffee](https://buymeacoffee.com/hudsonbrendon) ☕ or star the repo. Bug reports and pull
requests are welcome.

## Credits and license

MIT. Built with devkitPPC and [libogc2](https://github.com/extremscorner/libogc2). Games are started by
[Swiss](https://github.com/emukidid/swiss-gc) (GPL), which Cube Menu calls but does not include or modify.
Covers, disc art and metadata from [GameTDB](https://www.gametdb.com), screenshots from the
[libretro thumbnails](https://github.com/libretro-thumbnails/libretro-thumbnails), synopses from Wikipedia.
UI font: M PLUS Rounded 1c (OFL); wordmark: Jost (OFL). The emblem is original artwork in the spirit of the
console's logo. GameCube is a trademark of Nintendo; this is a fan project with no affiliation.
