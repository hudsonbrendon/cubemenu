# Building from source

Everything builds in the libogc2 container (devkitPPC + libogc2 + portlibs):

```bash
./build.sh                                   # regenerates data/ (fonts, logo), builds dist/cubemenu.dol
docker run --rm -v "$PWD":/src -w /src ghcr.io/extremscorner/libogc2:latest make -C apploader
python3 tools/mkiso.py dist/cubemenu.dol dist/boot.iso
sh tools/package.sh                          # dist/cubemenu-<version>.zip
```

`data/*.bin` (fonts, logo textures) are committed, so `make` alone works without Python. Regenerate them
with `python3 tools/mkassets.py` after changing `tools/mkfont.py` or `tools/mkbrand.py`.

## Layout

| Path | What |
|---|---|
| `source/main.c`, `app.c` | main loop, screens (intro, shelf / grid / list, details, settings, system, launch) |
| `source/gfx.c`, `font.c`, `ui.c` | GX 2D/3D helpers, text, background, glyphs, hints |
| `source/scene3d.c` | the GameCube case and the console models |
| `source/library.c`, `settings.c`, `ini.c`, `fs.c` | games, metadata, stats, settings, card mounting |
| `source/assets.c`, `clip.c`, `audio.c` | texture loader thread, clip streaming thread, MP3 + UI sounds |
| `source/boot.c`, `stub.S` | DOL chainloader (starts Swiss with arguments) |
| `apploader/` | the apploader in `boot.iso` |
| `tools/cube_card/` | the card tool (scan, fetch, build, install), GX texture encoders |
| `tools/mkiso.py`, `mktestdisc.py`, `tour.py` | disc images, emulator test disc, scripted screenshots / video |

## Testing in Dolphin

Dolphin has no SD adapter for the GameCube, so the test disc carries the library in its ISO 9660 part:

```bash
python3 tools/mktestdisc.py /Volumes/GAMECUBE dist/test.iso      # headers of the card's games + built assets
/Applications/Dolphin.app/Contents/MacOS/Dolphin -e dist/test.iso
```

Settings that matter: **DSP LLE** (the HLE DSP does not know libogc's AESND microcode and the menu waits
for it forever) and "speed up disc transfer rate". Swiss itself does not draw in Dolphin, so a launch
ends on a black screen there; on the console the game starts.

`tools/tour.py` drives the menu with `cubemenu/demo.txt` (lines of `<frames to wait> <BUTTON>`), dumps
frames and audio, and picks screenshots by the frame number the menu draws in demo mode.
