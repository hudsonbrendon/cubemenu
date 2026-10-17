"""Turn <sd>/cubemenu/src/<ID>/ downloads into what the console reads, in <sd>/cubemenu/games/<ID>/:

  case.gxt   full cover (back | spine | front), 512x344 CMPR   - the 3D case in the coverflow
  disc.gxt   disc label, 160x160 RGB5A3                         - the mini disc
  snap.gxt / title.gxt   screenshots, 320x240 CMPR
  clip.cmv   background clip: "CMV1" header + fixed-size CMPR frames (no decoding on the console)
  music.mp3  theme (copied)
  info.txt   title, developer, publisher, year, genre, players, synopses
"""
import os, re, shutil, struct, subprocess, sys, zipfile, io, urllib.request
import xml.etree.ElementTree as ET
import numpy as np
from PIL import Image
from tools.cube_card import gx

CLIP_W, CLIP_H, CLIP_FPS, CLIP_SECONDS = 320, 176, 20, 12
TDB_URL = "https://www.gametdb.com/wiitdb.zip?LANG=ORIG&GAMECUBE=1"


def _fresh(dst, *srcs):
    return os.path.exists(dst) and all(os.path.getmtime(dst) >= os.path.getmtime(s) for s in srcs if os.path.exists(s))


def load_tdb(cache_dir):
    path = os.path.join(cache_dir, "wiitdb.xml")
    if not os.path.exists(path):
        data = urllib.request.urlopen(urllib.request.Request(TDB_URL, headers={"User-Agent": "cubemenu/0.1"}), timeout=120).read()
        with zipfile.ZipFile(io.BytesIO(data)) as z: open(path, "wb").write(z.read("wiitdb.xml"))
    db = {}
    for game in ET.parse(path).getroot().iter("game"):
        gid = game.findtext("id") or ""
        if len(gid) != 6 or game.findtext("type") != "GameCube": continue
        loc = {l.get("lang"): l.findtext("title") for l in game.findall("locale")}
        date = game.find("date")
        inp = game.find("input")
        rating = game.find("rating")
        db[gid] = dict(title=loc.get("EN") or next(iter(loc.values()), ""), developer=game.findtext("developer") or "",
                       publisher=game.findtext("publisher") or "", year=(date.get("year") if date is not None else "") or "",
                       genre=(game.findtext("genre") or "").split(",")[-1].strip(), players=inp.get("players") if inp is not None else "",
                       rating=(f"{rating.get('type')} {rating.get('value')}" if rating is not None else ""))
    return db


def _clean(s):
    return re.sub(r"\s+", " ", s.replace("\r", " ").replace("\n", " ")).strip()


def write_info(g, src, out, tdb, flags):
    t = tdb.get(g["id"], {})
    lines = ["# Cube Menu game info (cube-card build)", f"id={g['id']}", f"title={g['name']}"]
    for k in ("developer", "publisher", "year", "players", "rating"):
        if t.get(k): lines.append(f"{k}={_clean(t[k])}")
    if t.get("genre"): lines.append("genre=" + t["genre"].strip().lower())      # the menu translates / capitalises
    for lang in ("pt", "en", "es"):
        p = os.path.join(src, f"synopsis.{lang}.txt")
        if os.path.exists(p):
            txt = open(p, encoding="utf-8").read().strip()
            paras = [_clean(x) for x in re.split(r"\n\s*\n|\n", txt) if x.strip()]
            lines.append(f"synopsis.{lang}=" + "\\n\\n".join(paras))
    for k, v in flags.items(): lines.append(f"{k}={int(v)}")
    with open(os.path.join(out, "info.txt"), "w", encoding="utf-8") as f: f.write("\n".join(lines) + "\n")


def case_texture(src_png, dst):
    img = Image.open(src_png).convert("RGB")
    gx.save(dst, img, gx.CMPR, 512, 344)


def disc_texture(src_png, dst):
    img = Image.open(src_png).convert("RGBA")
    gx.save(dst, img, gx.RGB5A3, 160, 160)


def shot_texture(src_png, dst):
    img = Image.open(src_png).convert("RGB")
    gx.save(dst, img, gx.CMPR, 320, 240)


def encode_clip(src, dst, log=print):
    """mp4 -> CMV1: 16:9 centre crop, 320x176, 20 fps, 12 s from 2 s in."""
    vf = f"crop='min(iw,ih*16/9)':'min(ih,iw*9/16)',scale={CLIP_W}:{CLIP_H}:flags=lanczos,fps={CLIP_FPS},eq=saturation=1.08"
    r = subprocess.run(["ffmpeg", "-v", "error", "-ss", "2", "-i", src, "-t", str(CLIP_SECONDS), "-vf", vf,
                        "-f", "rawvideo", "-pix_fmt", "rgba", "-"], capture_output=True)
    raw = r.stdout
    fsz = CLIP_W * CLIP_H * 4
    n = len(raw) // fsz
    if n < CLIP_FPS * 3: log(f"  !! clip too short: {src}"); return False
    frame_bytes = CLIP_W * CLIP_H // 2
    with open(dst + ".tmp", "wb") as f:
        f.write(b"CMV1" + struct.pack(">HHHHI16x", CLIP_W, CLIP_H, CLIP_FPS, n, frame_bytes))
        for i in range(n):
            a = np.frombuffer(raw, np.uint8, fsz, i * fsz).reshape(CLIP_H, CLIP_W, 4).copy()
            a[..., 3] = 255
            f.write(gx.cmpr_array(a))
    os.replace(dst + ".tmp", dst)
    return True


def build(sd, games, force=False, log=print):
    base = os.path.join(sd, "cubemenu")
    os.makedirs(base, exist_ok=True)
    tdb = load_tdb(os.path.join(base, "src"))
    for g in games:
        src = os.path.join(base, "src", g["id"])
        out = os.path.join(base, "games", g["id"])
        os.makedirs(out, exist_ok=True)
        flags = {}
        jobs = (("coverfull.png", "case.gxt", case_texture), ("disc.png", "disc.gxt", disc_texture),
                ("snap.png", "snap.gxt", shot_texture), ("title.png", "title.gxt", shot_texture))
        for s, d, fn in jobs:
            sp, dp = os.path.join(src, s), os.path.join(out, d)
            if not os.path.exists(sp): continue
            if force or not _fresh(dp, sp):
                try: fn(sp, dp)
                except Exception as e: log(f"  !! {g['id']} {s}: {e}"); continue
        flags["snap"] = os.path.exists(os.path.join(out, "snap.gxt"))
        flags["title_shot"] = os.path.exists(os.path.join(out, "title.gxt"))
        v, c = os.path.join(src, "video.mp4"), os.path.join(out, "clip.cmv")
        if os.path.exists(v) and (force or not _fresh(c, v)): encode_clip(v, c, log)
        flags["clip"] = os.path.exists(c)
        m, mo = os.path.join(src, "music.mp3"), os.path.join(out, "music.mp3")
        if os.path.exists(m) and (force or not _fresh(mo, m)): shutil.copyfile(m, mo)
        flags["music"] = os.path.exists(mo)
        write_info(g, src, out, tdb, flags)
        log(f"  ok {g['id']} {g['name']}: " + " ".join(k for k, v in flags.items() if v))
        sys.stdout.flush()
    os.makedirs(os.path.join(base, "music"), exist_ok=True)


SWISS_INI = os.path.join("swiss", "settings", "global.ini")
MENU_DOL = "/cubemenu/cubemenu.dol"


def set_swiss_autoload(sd, value):
    """Swiss's "Load at startup" (Autoload= in global.ini). The previous file is kept once as .cubemenu-bak."""
    ini = os.path.join(sd, SWISS_INI)
    if not os.path.exists(ini):
        print(f"  !! {ini} not found: open Swiss once and save its settings, then run install again")
        return False
    text = open(ini, newline="").read()
    bak = ini + ".cubemenu-bak"
    if not os.path.exists(bak): open(bak, "w", newline="").write(text)
    nl = "\r\n" if "\r\n" in text else "\n"
    lines = text.split(nl)
    if any(l.startswith("Autoload=") for l in lines):
        lines = [f"Autoload={value}" if l.startswith("Autoload=") else l for l in lines]
    else:
        end = next((i for i, l in enumerate(lines) if "Settings End" in l), len(lines))
        lines.insert(end, f"Autoload={value}")
    open(ini, "w", newline="").write(nl.join(lines))
    print(f"  Swiss will load {value or '(nothing)'} at startup")
    return True


def install(sd, iso, dol, mode="swiss"):
    """mode "swiss" (default): GC Loader keeps booting Swiss's own boot.iso and Swiss starts the menu
    at power on. mode "iso": Cube Menu's boot.iso replaces /boot.iso (Swiss's is kept)."""
    os.makedirs(os.path.join(sd, "cubemenu"), exist_ok=True)
    shutil.copyfile(dol, os.path.join(sd, "cubemenu", "cubemenu.dol"))
    print("  installed cubemenu/cubemenu.dol")
    boot = os.path.join(sd, "boot.iso")
    keep = os.path.join(sd, "swiss", "boot-swiss.iso")
    if mode == "iso":
        if os.path.exists(boot) and not os.path.exists(keep):
            with open(boot, "rb") as f: head = f.read(0x60)
            if b"Cube Menu" not in head:
                shutil.copyfile(boot, keep)
                print(f"  kept the previous boot.iso as {keep}")
        shutil.copyfile(iso, boot)
        set_swiss_autoload(sd, "")
        print(f"  installed {boot}")
        return
    # back to Swiss's boot.iso if an earlier install replaced it
    if os.path.exists(boot):
        with open(boot, "rb") as f: head = f.read(0x60)
        if b"Cube Menu" in head and os.path.exists(keep):
            shutil.copyfile(keep, boot)
            print("  restored Swiss's boot.iso")
    set_swiss_autoload(sd, "gcldr:" + MENU_DOL)
