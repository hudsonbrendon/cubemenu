"""cube-card: prepare a GameCube SD card (GC Loader / SD2SP2 / SD Gecko) for Cube Menu.

  python3 -m tools.cube_card scan    <sd_root>              list the games found under /games
  python3 -m tools.cube_card fetch   <sd_root> [--art] [--synopsis] [--videos] [--music] [--force] [--only ID,..]
  python3 -m tools.cube_card build   <sd_root>              convert everything in cubemenu/src into console assets
  python3 -m tools.cube_card install <sd_root> [--iso boot.iso] [--dol cubemenu.dol]   make Cube Menu boot first

Games live where Swiss expects them: /games/<Name>/game.iso (or disc1.iso + disc2.iso). The folder name is the
title shown in the menu. Downloads go to /cubemenu/src/<GAMEID>/, console-ready files to /cubemenu/games/<GAMEID>/.
"""
import argparse, os, re, sys
sys.path.insert(0, os.path.join(os.path.dirname(__file__), "..", ".."))
from tools.cube_card import fetch

IMAGE_EXT = (".iso", ".gcm")


def read_header(path):
    with open(path, "rb") as f: h = f.read(0x60)
    gid = h[:6].decode("ascii", "replace")
    if not re.fullmatch(r"[A-Z0-9]{6}", gid) or h[0x1C:0x20] != b"\xc2\x33\x9f\x3d": return None
    title = h[0x20:0x60].split(b"\0")[0].decode("latin-1").strip()
    return dict(id=gid, disc=h[6], title=title)


def display_name(folder):
    """'Mario Party 4 (v1.01)' -> 'Mario Party 4': revision and edition tags stay out of the menu."""
    return re.sub(r"\s*\((v\d[\d.]*|rev \w+|player's choice)\)", "", folder, flags=re.I).strip()


def scan(sd):
    games = {}
    root = os.path.join(sd, "games")
    for name in sorted(os.listdir(root), key=str.lower):
        folder = os.path.join(root, name)
        if name.startswith(".") or not os.path.isdir(folder): continue
        for fn in sorted(os.listdir(folder)):
            if fn.startswith(".") or not fn.lower().endswith(IMAGE_EXT): continue
            h = read_header(os.path.join(folder, fn))
            if not h: print(f"  !! {name}/{fn}: not a GameCube image"); continue
            g = games.setdefault(h["id"], dict(id=h["id"], name=display_name(name), title=h["title"], folder=folder, discs=[]))
            g["discs"].append((h["disc"], fn))
    for g in games.values(): g["discs"].sort()
    return sorted(games.values(), key=lambda g: g["name"].lower())


def cmd_scan(a):
    for g in scan(a.sd):
        print(f"{g['id']}  {g['name']:<48} {', '.join(fn for _, fn in g['discs'])}")


def cmd_fetch(a):
    games = scan(a.sd)
    if a.only: games = [g for g in games if g["id"] in a.only.split(",")]
    every = not (a.art or a.synopsis or a.videos or a.music)
    for g in games:
        src = os.path.join(a.sd, "cubemenu", "src", g["id"])
        os.makedirs(src, exist_ok=True)
        try:
            if every or a.art: fetch.fetch_tdb(g, src, a.force); fetch.fetch_shots(g, src, a.force)
            if every or a.synopsis: fetch.fetch_synopses(g, src, a.force)
            if every or a.videos: fetch.fetch_video(g, src, a.force)
            if every or a.music: fetch.fetch_music(g, src, a.force)
        except Exception as e:
            print(f"  !! {g['id']} {g['name']}: {e}"[:300])
        sys.stdout.flush()


def cmd_build(a):
    from tools.cube_card import build
    build.build(a.sd, scan(a.sd), force=a.force)


def cmd_install(a):
    from tools.cube_card import build
    build.install(a.sd, a.iso, a.dol)


def main():
    p = argparse.ArgumentParser(prog="cube-card")
    sub = p.add_subparsers(dest="cmd", required=True)
    s = sub.add_parser("scan"); s.add_argument("sd"); s.set_defaults(fn=cmd_scan)
    s = sub.add_parser("fetch"); s.add_argument("sd")
    for flag in ("--art", "--synopsis", "--videos", "--music", "--force"): s.add_argument(flag, action="store_true")
    s.add_argument("--only"); s.set_defaults(fn=cmd_fetch)
    s = sub.add_parser("build"); s.add_argument("sd"); s.add_argument("--force", action="store_true"); s.set_defaults(fn=cmd_build)
    s = sub.add_parser("install"); s.add_argument("sd"); s.add_argument("--iso", default="dist/boot.iso")
    s.add_argument("--dol", default="dist/cubemenu.dol"); s.set_defaults(fn=cmd_install)
    a = p.parse_args()
    a.fn(a)


if __name__ == "__main__":
    main()
