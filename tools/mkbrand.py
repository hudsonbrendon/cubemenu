"""Cube Menu brand: an isometric cube-in-a-cube emblem (in the spirit of the GameCube logo) over the
CUBE MENU wordmark (Jost Bold, a geometric sans in the vein of the console's lettering). Writes assets/brand/*.png and the textures the menu embeds (data/logo*.bin)."""
import math, os, sys
from PIL import Image, ImageDraw, ImageFont, ImageFilter
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
from tools.cube_card import gx

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
INDIGO = ((150, 136, 236), (98, 82, 200), (62, 49, 150))      # top, left, right faces
JOST = os.path.join(ROOT, "assets", "fonts", "Jost.ttf")      # geometric sans, OFL


def hexpts(cx, cy, s):
    c, h = math.cos(math.pi / 6) * s, s / 2
    return dict(c=(cx, cy), top=(cx, cy - s), ur=(cx + c, cy - h), lr=(cx + c, cy + h), bot=(cx, cy + s),
                ll=(cx - c, cy + h), ul=(cx - c, cy - h))


def faces(p):
    return ([p["c"], p["ul"], p["top"], p["ur"]],        # top
            [p["c"], p["ul"], p["ll"], p["bot"]],         # left
            [p["c"], p["ur"], p["lr"], p["bot"]])         # right


def emblem(size, colors=INDIGO, ss=4):
    """Big cube with a cube-shaped hollow at its front corner and a small cube sitting inside it.
    The band left between them is open on the right face, so the outline reads as a 'C' / 'G'."""
    S = size * ss
    img = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    mask = Image.new("L", (S, S), 0)
    d, m = ImageDraw.Draw(img), ImageDraw.Draw(mask)
    cx, cy, s = S / 2, S / 2, S * 0.48
    big = hexpts(cx, cy, s)
    for poly, col in zip(faces(big), colors): d.polygon(poly, fill=col + (255,))
    # hollow: same-centre cube, 64% size, cut out (with the opening slot on the right face)
    hole = hexpts(cx, cy, s * 0.64)
    m.polygon([hole["top"], hole["ur"], hole["lr"], hole["bot"], hole["ll"], hole["ul"]], fill=255)
    # slot through the right face: from the hollow to the outer edge, lower half
    k = math.cos(math.pi / 6)
    w = s * 0.13
    p0 = (cx + k * s * 0.60, cy + s * 0.02)
    m.polygon([(p0[0], p0[1] - w * 0.2), (cx + k * s * 1.02, cy + s * 0.02 - w * 0.2 + s * 0.0),
               (cx + k * s * 1.02, cy + s * 0.02 + w * 1.0), (p0[0], p0[1] + w * 1.0)], fill=255)
    img.putalpha(Image.composite(Image.new("L", (S, S), 0), img.getchannel("A"), mask))
    # small cube inside the hollow, 40%
    small = hexpts(cx, cy, s * 0.40)
    sm = Image.new("RGBA", (S, S), (0, 0, 0, 0))
    sd = ImageDraw.Draw(sm)
    for poly, col in zip(faces(small), colors): sd.polygon(poly, fill=col + (255,))
    img = Image.alpha_composite(img, sm)
    return img.resize((size, size), Image.LANCZOS)


def wordmark(text, height, color, tracking=0.06):
    font = ImageFont.truetype(JOST, int(height * 1.32))
    font.set_variation_by_name("Bold")
    asc, desc = font.getmetrics()
    widths = [font.getlength(ch) for ch in text]
    track = height * tracking
    W = int(sum(widths) + track * (len(text) - 1) + height)
    img = Image.new("RGBA", (W, asc + desc + 8), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    x = height / 2
    for ch, w in zip(text, widths):
        d.text((x, 4), ch, font=font, fill=color)
        x += w + track
    return img.crop(img.getbbox())


def logo(width, text_color, sub_color=None, emblem_colors=INDIGO):
    em = emblem(int(width * 0.46), emblem_colors)
    top = wordmark("CUBE MENU", int(width * 0.105), text_color, 0.05)
    if top.width > width: top = top.resize((width, int(top.height * width / top.width)), Image.LANCZOS)
    gap = int(width * 0.05)
    H = em.height + gap + top.height
    img = Image.new("RGBA", (width, H), (0, 0, 0, 0))
    img.alpha_composite(em, ((width - em.width) // 2, 0))
    img.alpha_composite(top, ((width - top.width) // 2, em.height + gap))
    return img


def pad_to(img, w, h):
    out = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    img = img.copy()
    img.thumbnail((w, h), Image.LANCZOS)
    out.alpha_composite(img, ((w - img.width) // 2, (h - img.height) // 2))
    return out


if __name__ == "__main__":
    bdir, ddir = os.path.join(ROOT, "assets", "brand"), os.path.join(ROOT, "data")
    os.makedirs(bdir, exist_ok=True); os.makedirs(ddir, exist_ok=True)
    logo(1024, (98, 82, 200, 255)).save(os.path.join(bdir, "logo.png"))
    logo(1024, (255, 255, 255, 255)).save(os.path.join(bdir, "logo_white.png"))
    emblem(512).save(os.path.join(bdir, "emblem.png"))
    # README banner: logo on indigo
    ban = Image.new("RGBA", (1280, 560), (20, 15, 51, 255))
    g = Image.new("L", (1, 560)); [g.putpixel((0, y), int(255 * y / 559)) for y in range(560)]
    ban = Image.composite(Image.new("RGBA", ban.size, (58, 46, 128, 255)), ban, g.resize(ban.size))
    lg = logo(560, (255, 255, 255, 255))
    ban.alpha_composite(lg, ((1280 - lg.width) // 2, (560 - lg.height) // 2))
    ban.convert("RGB").save(os.path.join(bdir, "banner.png"))
    # embedded textures
    def save_tex(img, name):
        open(os.path.join(ddir, name), "wb").write(gx.header(img.width, img.height, gx.RGB5A3, img.width * img.height * 2) + gx.rgb5a3(img))
    save_tex(pad_to(logo(320, (255, 255, 255, 255)), 320, 288), "logo.bin")
    save_tex(emblem(128), "emblem.bin")
    wm = wordmark("CUBE MENU", 40, (255, 255, 255, 255), 0.05)
    save_tex(pad_to(wm, 256, 40), "wordmark.bin")
    print("brand OK")
