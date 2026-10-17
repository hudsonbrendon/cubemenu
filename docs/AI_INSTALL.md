# Installing Cube Menu with an AI agent

A runbook for an agent (Claude Code, Codex, Cursor...) with shell access to the computer where the SD
card is mounted. Follow it in order and report what you did.

1. **Find the card.** Ask the user for the mount point if it was not given (macOS `/Volumes/<name>`,
   Linux `/media/<user>/<name>`, Windows a drive letter). Check it has `/games` with one folder per game
   (`game.iso`, or `disc1.iso` + `disc2.iso`). Do not move or rename the user's games without asking.
2. **Check Swiss.** `/swiss/boot.dol` must exist. If it does not, tell the user to download Swiss
   (https://github.com/emukidid/swiss-gc/releases) and copy `swiss_rXXXX.dol` there as `boot.dol`.
3. **Get Cube Menu.** Download the latest `cubemenu-<version>.zip` from
   https://github.com/hudsonbrendon/cubemenu/releases and unzip it; run the next commands from that folder.
4. **Requirements.** Python 3 with Pillow and numpy (`python3 -m pip install pillow numpy`); `ffmpeg` and
   `yt-dlp` for clips and music (Homebrew, apt, winget...).
5. **Scan**: `python3 -m tools.cube_card scan <card>`. Every game should be listed with its ID. Report
   the ones that are not (bad dumps, compressed formats Swiss does not read).
6. **Fetch**: `python3 -m tools.cube_card fetch <card>`. Takes a few minutes per ten games (clips and
   music are the slow part). Lines starting with `??` are things that were not found; they are optional.
7. **Build**: `python3 -m tools.cube_card build <card>`.
8. **Install** (GC Loader): `python3 -m tools.cube_card install <card> --iso boot.iso --dol cubemenu.dol`.
   This replaces `/boot.iso` and keeps the previous one as `/swiss/boot-swiss.iso`. Tell the user that
   copying it back to `/boot.iso` restores plain Swiss. For SD2SP2 / SD Gecko users, skip this and tell
   them to start `cubemenu/cubemenu.dol` the way they start other DOLs.
9. **Verify**: `/boot.iso` starts with the ID `GCMEHB`; `/cubemenu/games/<ID>/case.gxt` exists for most
   games. On macOS, `dot_clean -m <card>` removes the `._*` files.
10. Tell the user to eject the card and turn the console on.
