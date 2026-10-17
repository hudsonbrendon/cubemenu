"""Bake the menu fonts into data/font_<name>.bin: glyph table for Latin-1 (32..255) + IA8 atlas.

Layout: "FNT1", u16 line height, u16 ascent, u16 atlas w, u16 atlas h, 4 pad,
224 x {u16 x, u16 y, u8 w, u8 h, s8 xoff, s8 yoff, u8 advance, 3 pad} (12 bytes), pad to 32, IA8 atlas.
"""
import os, struct, sys
from PIL import Image, ImageDraw, ImageFont
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))
from tools.cube_card import gx

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
FONTS = {  # name: (ttf, pixel size)
    "s": ("MPLUSRounded1c-Bold.ttf", 17),
    "m": ("MPLUSRounded1c-Bold.ttf", 22),
    "l": ("MPLUSRounded1c-ExtraBold.ttf", 32),
    "xl": ("MPLUSRounded1c-ExtraBold.ttf", 50),
}
FIRST, LAST = 32, 255


def bake(ttf, size, out):
    font = ImageFont.truetype(os.path.join(ROOT, "assets", "fonts", ttf), size)
    ascent, descent = font.getmetrics()
    glyphs = []
    for cp in range(FIRST, LAST + 1):
        ch = chr(cp)
        if 0x7F <= cp < 0xA0: glyphs.append(None); continue
        box = font.getbbox(ch)
        adv = round(font.getlength(ch))
        if box[2] <= box[0] or box[3] <= box[1]: glyphs.append((None, 0, 0, adv)); continue
        img = Image.new("L", (box[2] - box[0] + 2, box[3] - box[1] + 2), 0)
        ImageDraw.Draw(img).text((1 - box[0], 1 - box[1]), ch, font=font, fill=255)
        glyphs.append((img, box[0] - 1, box[1] - 1, adv))
    W = 512
    x = y = row = 0
    place = []
    for g in glyphs:
        if not g or g[0] is None: place.append(None); continue
        w, h = g[0].size
        if x + w > W: x, y, row = 0, y + row + 1, 0
        place.append((x, y)); x += w + 1; row = max(row, h)
    H = y + row + 1
    H = (H + 3) // 4 * 4
    atlas = Image.new("LA", (W, H), (255, 0))
    for g, p in zip(glyphs, place):
        if p: atlas.paste(Image.merge("LA", (Image.new("L", g[0].size, 255), g[0])), p)
    table = b""
    for g, p in zip(glyphs, place):
        if not g: table += struct.pack(">HHBBbbB3x", 0, 0, 0, 0, 0, 0, 0)
        elif not p: table += struct.pack(">HHBBbbB3x", 0, 0, 0, 0, 0, 0, g[3])
        else: table += struct.pack(">HHBBbbB3x", p[0], p[1], g[0].size[0], g[0].size[1], g[1], g[2], g[3])
    head = b"FNT1" + struct.pack(">HHHH4x", ascent + descent, ascent, W, H) + table
    head += b"\0" * (-len(head) % 32)
    with open(out, "wb") as f: f.write(head + gx.ia8(atlas.convert("RGBA"), W, H))
    return W, H


if __name__ == "__main__":
    os.makedirs(os.path.join(ROOT, "data"), exist_ok=True)
    for name, (ttf, size) in FONTS.items():
        print(name, bake(ttf, size, os.path.join(ROOT, "data", f"font_{name}.bin")))
