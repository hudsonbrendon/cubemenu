# Preparing the SD card

## Requirements

- A GameCube that boots Swiss from an SD card: **GC Loader** (the menu ships as its `boot.iso`), or an
  SD2SP2 / SD Gecko setup where you can start a DOL.
- Swiss's `boot.dol` at `/swiss/boot.dol` (also found at `/swiss/swiss.dol`, `/autoexec.dol`, or
  `/cubemenu/swiss.dol` if you want a specific build only for the menu).
- On the computer: Python 3 with Pillow and numpy (`pip install pillow numpy`). For clips and music:
  `ffmpeg` and `yt-dlp` (`brew install ffmpeg yt-dlp` on macOS). `mkisofs` only to build `boot.iso` yourself.

## Layout

```
/boot.iso                         Cube Menu for GC Loader
/games/<Name>/game.iso            one folder per game; the folder name is the title in the menu
/games/<Name>/disc1.iso, disc2.iso   multi-disc games (the header's disc number decides the order)
/swiss/boot.dol                   Swiss
/cubemenu/src/<GAMEID>/           downloads (cube-card fetch)
/cubemenu/games/<GAMEID>/         console files (cube-card build)
/cubemenu/music/*.mp3             optional menu music, played when a game has no theme
/cubemenu/settings.ini            written by the menu
/cubemenu/stats.ini               play time, launches, favourites, collections, achievements
/cubemenu/backup/                 memory card backups (.gci)
/apps/<Name>/*.dol, *.dol+cli     homebrew; cube-card build makes its cover and description
```

`.iso` and `.gcm` images are read. Tags such as `(v1.01)` or `(Player's Choice)` in the folder name are
hidden in the menu. Swiss's own options for each game (video mode, cheats, patches) still apply, since
Swiss is the one that boots it.

## The card tool

```bash
python3 -m tools.cube_card scan    <card>    # games found under /games, with their IDs
python3 -m tools.cube_card fetch   <card>    # everything below
python3 -m tools.cube_card fetch   <card> --art --synopsis     # just covers, discs, screenshots, synopses
python3 -m tools.cube_card fetch   <card> --videos --music     # just clips and themes
python3 -m tools.cube_card fetch   <card> --only GALE01,GM8E01 --force   # redo some games
python3 -m tools.cube_card build   <card>    # convert for the console (only what changed)
python3 -m tools.cube_card install  <card> --iso boot.iso --dol cubemenu.dol
```

What `fetch` gets, per game ID:

| File in `cubemenu/src/<ID>/` | Source |
|---|---|
| `coverfull.png` (back, spine, front) | GameTDB `coverfullHQ` / `coverfull` |
| `disc.png` | GameTDB disc art |
| `snap.png`, `title.png` | libretro thumbnails (Nintendo - GameCube) |
| `synopsis.pt.txt`, `.en.txt`, `.es.txt` | Wikipedia, lead section of the game's article |
| `video.mp4` | yt-dlp: a gameplay video (or a trailer), 20 s, no sound |
| `music.mp3` | yt-dlp: the game's main theme, 60 s |
| `hltb.txt` | HowLongToBeat: main story, + extras, completionist |
| `manual/p000.jpg`… | archive.org: a scan of the manual (English, then Spanish) |

You can drop your own files with those names in that folder and run `build`; nothing you put there is
downloaded again unless you pass `--force`.

`build` writes, in `cubemenu/games/<ID>/`: `case.gxt` (512x344 CMPR), `disc.gxt` (160x160 RGB5A3),
`snap.gxt` / `title.gxt` (320x240 CMPR), `clip.cmv` (CMPR frames, 320x176, 20 fps, 12 s), `music.mp3` and
`info.txt` (title, developer, publisher, year, genre, players, synopses). Metadata comes from the
GameTDB database (`cubemenu/src/wiitdb.xml`, downloaded once).

## Space

About 7 MB per game with a clip (the clip is 6.4 MB), 1 MB without.

## Troubleshooting

- **The menu says no card**: the card is read from GC Loader, SD2SP2, SD Gecko B and A, in that order,
  and needs a `/games` or `/cubemenu` folder.
- **A game shows a blank case**: no cover was found for its ID; put a `coverfull.png` (back | spine | front)
  in `cubemenu/src/<ID>/` and run `build`.
- **"Swiss not found"**: copy Swiss's `boot.dol` to `/swiss/boot.dol`.
- **Back to plain Swiss**: copy `swiss/boot-swiss.iso` (kept by `install`) to `/boot.iso`.
- macOS leaves `._*` files on FAT cards; the menu ignores them. `dot_clean -m /Volumes/<card>` removes them.
