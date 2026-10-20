"""Homebrew in <sd>/apps/<folder>/ (one folder per app, its .dol / .dol+cli files): a generated case cover
and an info.txt in <sd>/cubemenu/games/<ID>/, where ID is "~" + the first five letters or digits of the
folder name in capitals (the menu computes the same ID).
"""
import colorsys, hashlib, os, re, textwrap, xml.etree.ElementTree as ET
from PIL import Image, ImageDraw, ImageFont
from tools.cube_card import gx

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), "..", ".."))
FONT_TITLE = os.path.join(ROOT, "assets", "fonts", "Jost.ttf")
FONT_TEXT = os.path.join(ROOT, "assets", "fonts", "MPLUSRounded1c-Medium.ttf")
FONT_BOLD = os.path.join(ROOT, "assets", "fonts", "MPLUSRounded1c-ExtraBold.ttf")

# what the folders on a typical card hold; anything else gets its meta.xml / README, or just its name
KNOWN = {
    "gbi": dict(title="Game Boy Interface", developer="Extrems", genre="emulator", color=(74, 63, 160),
                pt="Usa o Game Boy Player para rodar cartuchos de Game Boy, Game Boy Color e Game Boy Advance com vídeo de baixa latência, escala sem borrão e saída para scalers como OSSC e RetroTINK. Cada versão no menu é um perfil de vídeo diferente.",
                en="Uses the Game Boy Player to run Game Boy, Game Boy Color and Game Boy Advance cartridges with low-latency video, clean scaling and output profiles for scalers such as the OSSC and RetroTINK. Each version in the menu is a different video profile.",
                es="Usa el Game Boy Player para ejecutar cartuchos de Game Boy, Game Boy Color y Game Boy Advance con vídeo de baja latencia, escalado limpio y perfiles para escaladores como OSSC y RetroTINK. Cada versión del menú es un perfil de vídeo distinto."),
    "gbihf": dict(title="Game Boy Interface HF", developer="Extrems", genre="emulator", color=(52, 92, 168),
                  pt="Edição de alta fidelidade do Game Boy Interface: mais opções de cor, filtros e perfis para scalers e TVs modernas. Precisa do Game Boy Player.",
                  en="High-fidelity edition of Game Boy Interface: more colour options, filters and profiles for scalers and modern TVs. Needs the Game Boy Player.",
                  es="Edición de alta fidelidad de Game Boy Interface: más opciones de color, filtros y perfiles para escaladores y televisores modernos. Necesita el Game Boy Player."),
    "gbisr": dict(title="Game Boy Interface SR", developer="Extrems", genre="emulator", color=(40, 120, 110),
                  pt="Edição para speedrun do Game Boy Interface, com o tempo dos jogos fiel ao Game Boy original. Precisa do Game Boy Player.",
                  en="Speedrun edition of Game Boy Interface, with game timing true to the original Game Boy. Needs the Game Boy Player.",
                  es="Edición para speedrun de Game Boy Interface, con el tiempo de los juegos fiel a la Game Boy original. Necesita el Game Boy Player."),
    "snes": dict(title="Snes9x TX", developer="Snes9x GX / TX", genre="emulator", color=(88, 80, 140),
                 pt="Emulador de Super Nintendo baseado no Snes9x GX. Lê as ROMs da pasta snes9xtx/roms de um SD no SD2SP2 ou SD Gecko.",
                 en="Super Nintendo emulator based on Snes9x GX. Reads ROMs from snes9xtx/roms on an SD card in an SD2SP2 or SD Gecko.",
                 es="Emulador de Super Nintendo basado en Snes9x GX. Lee las ROM de snes9xtx/roms en una SD en un SD2SP2 o SD Gecko."),
    "nes": dict(title="FCEUX TX", developer="FCE Ultra GX / TX", genre="emulator", color=(170, 40, 46),
                pt="Emulador de NES baseado no FCE Ultra GX. Lê as ROMs de um SD no SD2SP2 ou SD Gecko.",
                en="NES emulator based on FCE Ultra GX. Reads ROMs from an SD card in an SD2SP2 or SD Gecko.",
                es="Emulador de NES basado en FCE Ultra GX. Lee las ROM de una SD en un SD2SP2 o SD Gecko."),
    "genesis": dict(title="Genesis Plus", developer="Eke-Eke", genre="emulator", color=(30, 30, 36),
                    pt="Emulador de Mega Drive, Master System e Game Gear (Genesis Plus GX). Lê as ROMs do disco.",
                    en="Mega Drive / Genesis, Master System and Game Gear emulator (Genesis Plus GX). Reads ROMs from the disc.",
                    es="Emulador de Mega Drive, Master System y Game Gear (Genesis Plus GX). Lee las ROM del disco."),
    "openhcl": dict(title="Hydra Castle Labyrinth", developer="E. Hashimoto / gameblabla", genre="platformer", color=(36, 96, 70),
                    pt="Port de GameCube do OpenHCL, a versão de código aberto de Hydra Castle Labyrinth: um metroidvania em pixel art cheio de segredos.",
                    en="GameCube port of OpenHCL, the open-source version of Hydra Castle Labyrinth: a pixel-art metroidvania full of secrets.",
                    es="Port para GameCube de OpenHCL, la versión de código abierto de Hydra Castle Labyrinth: un metroidvania en pixel art lleno de secretos."),
    "wiicross": dict(title="Piero's Wiicross", developer="Piero", genre="puzzle", year="2008", color=(200, 120, 30),
                     pt="Picross: pinte as casas da grade seguindo os números das bordas até revelar o desenho escondido.",
                     en="Picross: colour the cells of the grid by the numbers on its edges until the hidden picture shows.",
                     es="Picross: colorea las casillas de la cuadrícula según los números de los bordes hasta revelar el dibujo oculto."),
    "methane": dict(title="Super Methane Brothers", developer="Apache Software / port GC", genre="platformer", color=(40, 110, 190),
                    pt="Plataforma de ação para dois jogadores: prenda os inimigos em bolhas de gás e estoure-os antes que escapem.",
                    en="Two-player action platformer: trap the baddies in gas bubbles and pop them before they escape.",
                    es="Plataformas de acción para dos jugadores: atrapa a los enemigos en burbujas de gas y revientalas antes de que escapen."),
}


def app_id(folder, taken):
    base = "~" + "".join(c for c in folder if c.isalnum()).upper()[:5]
    gid, k = base, len(base)
    for tries in range(10):
        if gid not in taken: return gid
        gid = (gid + str(tries))[:6] if k < 6 else gid[:5] + str(tries)
        k = min(k + 1, 6)
    return gid


def has_exe(path):
    return any(f.lower().endswith((".dol", ".dol+cli")) and not f.startswith(".") for f in os.listdir(path))


def apps(sd, taken=()):
    """[(id, folder)] in the order the menu reads them (directory order), skipping folders without a DOL."""
    root = os.path.join(sd, "apps")
    if not os.path.isdir(root): return []
    out, seen = [], set(taken)
    for name in os.listdir(root):
        p = os.path.join(root, name)
        if name.startswith(".") or not os.path.isdir(p) or not has_exe(p): continue
        gid = app_id(name, seen)
        seen.add(gid)
        out.append((gid, name))
    return out


def meta(folder_path, name):
    m = dict(KNOWN.get(name.lower(), {}))
    mx = os.path.join(folder_path, "meta.xml")          # Homebrew Channel style
    if os.path.exists(mx):
        try:
            r = ET.parse(mx).getroot()
            m.setdefault("title", (r.findtext("name") or "").strip())
            m.setdefault("developer", (r.findtext("coder") or r.findtext("author") or "").strip())
            d = (r.findtext("long_description") or r.findtext("short_description") or "").strip()
            if d: m.setdefault("en", d)
        except ET.ParseError: pass
    if "en" not in m:
        for f in sorted(os.listdir(folder_path)):
            if f.lower().startswith("readme"):
                txt = open(os.path.join(folder_path, f), encoding="utf-8", errors="replace").read()
                paras = [re.sub(r"\s+", " ", p).strip(" #") for p in re.split(r"\n\s*\n", txt)]
                paras = [p for p in paras if len(p) > 60]
                if paras: m["en"] = paras[0][:600]
                break
    m.setdefault("title", name)
    m.setdefault("genre", "homebrew")
    if "color" not in m:
        h = int(hashlib.md5(name.encode()).hexdigest()[:4], 16) / 65535
        m["color"] = tuple(int(c * 255) for c in colorsys.hsv_to_rgb(h, 0.55, 0.62))
    return m


def _font(path, size):
    return ImageFont.truetype(path, size)


def _fit(draw, text, path, size, width):
    while size > 10:
        f = _font(path, size)
        if draw.textlength(text, font=f) <= width: return f
        size -= 2
    return _font(path, size)


def cover(m, icon_path, out_png):
    """A keep-case cover, 1024 x 688: back | spine | front, the front from 0.5254 of the width."""
    W, H, SP0, SP1 = 1024, 688, 486, 538
    base = m["color"]
    dark = tuple(int(c * 0.35) for c in base)
    img = Image.new("RGB", (W, H))
    px = img.load()
    for y in range(H):
        t = y / H
        c = tuple(int(base[i] * (1 - t) + dark[i] * t) for i in range(3))
        for x in range(W): px[x, y] = c
    d = ImageDraw.Draw(img)
    # top band, like the console's covers
    for x0, x1 in ((0, SP0), (SP1, W)):
        d.rectangle([x0, 0, x1, 58], fill=(232, 232, 236))
        f = _font(FONT_BOLD, 26)
        d.text(((x0 + x1) / 2, 30), "HOMEBREW", font=f, fill=(40, 36, 70), anchor="mm")
    # front: icon, then the name
    fx0, fw = SP1, W - SP1
    if icon_path and os.path.exists(icon_path):
        ic = Image.open(icon_path).convert("RGBA")
        s = min(360 / ic.width, 220 / ic.height)
        ic = ic.resize((max(1, int(ic.width * s)), max(1, int(ic.height * s))), Image.NEAREST if ic.width < 160 else Image.LANCZOS)
        img.paste(ic, (fx0 + (fw - ic.width) // 2, 150 + (220 - ic.height) // 2), ic)
    else:
        cx, cy, r = fx0 + fw // 2, 260, 120
        d.ellipse([cx - r, cy - r, cx + r, cy + r], fill=tuple(min(255, int(c * 1.5) + 30) for c in base))
        initials = "".join(w[0] for w in re.findall(r"[A-Za-z0-9]+", m["title"])[:2]).upper()
        d.text((cx, cy), initials, font=_font(FONT_TITLE, 130), fill=(255, 255, 255), anchor="mm")
    lines = textwrap.wrap(m["title"], 18)[:3]
    y = 470
    for line in lines:
        f = _fit(d, line, FONT_TITLE, 58, fw - 60)
        d.text((fx0 + fw / 2, y), line, font=f, fill=(255, 255, 255), anchor="mm")
        y += 64
    if m.get("developer"):
        d.text((fx0 + fw / 2, H - 40), m["developer"], font=_fit(d, m["developer"], FONT_TEXT, 26, fw - 60), fill=(220, 220, 230), anchor="mm")
    # spine
    sp = Image.new("RGBA", (H - 80, SP1 - SP0), (0, 0, 0, 0))
    ds = ImageDraw.Draw(sp)
    ds.text(((H - 80) / 2, (SP1 - SP0) / 2), m["title"], font=_fit(ds, m["title"], FONT_TITLE, 34, H - 120), fill=(255, 255, 255), anchor="mm")
    img.paste(sp.rotate(-90, expand=True), (SP0, 60), sp.rotate(-90, expand=True))
    d.rectangle([SP0, 0, SP0 + 2, H], fill=dark)
    d.rectangle([SP1 - 2, 0, SP1, H], fill=dark)
    # back: the description
    txt = m.get("pt") or m.get("en") or ""
    y = 100
    for line in textwrap.wrap(txt, 30)[:13]:
        d.text((40, y), line, font=_font(FONT_TEXT, 26), fill=(235, 235, 240))
        y += 38
    img.save(out_png)


def write_info(gid, m, out, flags):
    lines = ["# Cube Menu app info (cube-card build)", f"id={gid}", f"title={m['title']}", "genre=" + m["genre"]]
    for k in ("developer", "year"):
        if m.get(k): lines.append(f"{k}={m[k]}")
    for lang in ("pt", "en", "es"):
        if m.get(lang): lines.append(f"synopsis.{lang}=" + re.sub(r"\s+", " ", m[lang]).strip())
    for k, v in flags.items(): lines.append(f"{k}={int(v)}")
    with open(os.path.join(out, "info.txt"), "w", encoding="utf-8") as f: f.write("\n".join(lines) + "\n")


def build(sd, taken=(), log=print):
    base = os.path.join(sd, "cubemenu")
    for gid, name in apps(sd, taken):
        path = os.path.join(sd, "apps", name)
        m = meta(path, name)
        src = os.path.join(base, "src", gid)
        out = os.path.join(base, "games", gid)
        os.makedirs(src, exist_ok=True)
        os.makedirs(out, exist_ok=True)
        icon = os.path.join(path, "icon.png")
        png = os.path.join(src, "coverfull.png")
        user_cover = os.path.exists(png) and not os.path.exists(png + ".generated")
        if not user_cover:              # a coverfull.png of your own is kept
            cover(m, icon, png)
            open(png + ".generated", "w").close()
        gx.save(os.path.join(out, "case.gxt"), Image.open(png).convert("RGB"), gx.CMPR, 512, 344)
        write_info(gid, m, out, {"snap": False, "title_shot": False, "clip": False, "music": False})
        log(f"  ok {gid} {m['title']} (app)")
