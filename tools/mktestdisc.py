"""Emulator test disc: Dolphin has no SD adapter for the GameCube, so the menu also reads a library
from an ISO9660 disc. This packs the card's library (game headers only) + built assets + Swiss into
a bootable image.

  python3 tools/mktestdisc.py <sd_root> dist/test.iso [--demo tour.txt] [--only ID,ID] [--settings file]
"""
import argparse, os, shutil, sys, tempfile
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
from tools.cube_card.__main__ import scan
from tools import mkiso


def main():
    p = argparse.ArgumentParser()
    p.add_argument("sd"); p.add_argument("out"); p.add_argument("--demo"); p.add_argument("--only")
    p.add_argument("--settings"); p.add_argument("--dol", default="dist/cubemenu.dol"); p.add_argument("--stats")
    p.add_argument("--no-clips", action="store_true"); p.add_argument("--swiss", help="DOL to put at /swiss/boot.dol")
    a = p.parse_args()
    games = scan(a.sd)
    if a.only: games = [g for g in games if g["id"] in a.only.split(",")]
    with tempfile.TemporaryDirectory() as tmp:
        for g in games:
            folder = os.path.join(tmp, "games", os.path.basename(g["folder"]))
            os.makedirs(folder, exist_ok=True)
            for _, fn in g["discs"]:
                with open(os.path.join(g["folder"], fn), "rb") as f: head = f.read(0x440)
                open(os.path.join(folder, fn), "wb").write(head)
            src = os.path.join(a.sd, "cubemenu", "games", g["id"])
            if os.path.isdir(src):
                dst = os.path.join(tmp, "cubemenu", "games", g["id"])
                os.makedirs(dst, exist_ok=True)
                for fn in os.listdir(src):
                    if fn.startswith("."): continue
                    if a.no_clips and fn == "clip.cmv": continue
                    shutil.copyfile(os.path.join(src, fn), os.path.join(dst, fn))
        music = os.path.join(a.sd, "cubemenu", "music")
        if os.path.isdir(music):
            shutil.copytree(music, os.path.join(tmp, "cubemenu", "music"), ignore=shutil.ignore_patterns(".*"))
        os.makedirs(os.path.join(tmp, "cubemenu"), exist_ok=True)
        if a.demo: shutil.copyfile(a.demo, os.path.join(tmp, "cubemenu", "demo.txt"))
        if a.settings: shutil.copyfile(a.settings, os.path.join(tmp, "cubemenu", "settings.ini"))
        if a.stats: shutil.copyfile(a.stats, os.path.join(tmp, "cubemenu", "stats.ini"))
        swiss = a.swiss or os.path.join(a.sd, "swiss", "boot.dol")
        if os.path.exists(swiss):
            os.makedirs(os.path.join(tmp, "swiss"), exist_ok=True)
            shutil.copyfile(swiss, os.path.join(tmp, "swiss", "boot.dol"))
        off, size = mkiso.build(a.dol, a.out, tmp, region=1)
        print(f"{a.out}: {len(games)} games, {size >> 20} MB")


if __name__ == "__main__":
    main()
