"""GameCube GX texture encoders (numpy) and the .gxt container the menu loads.

.gxt = 32-byte header ("GXT1", u16 width, u16 height, u8 GX format, 3 pad, u32 data size, 16 pad) + tiled data.
CMPR is S3TC/DXT1 laid out in 8x8 tiles of four 4x4 blocks, big-endian colours, first pixel in the top bits.
"""
import struct
import numpy as np
from PIL import Image

I8, IA8, RGB565, RGB5A3, RGBA8, CMPR = 0x1, 0x3, 0x4, 0x5, 0x6, 0xE
MAGIC = b"GXT1"


def header(w, h, fmt, size):
    return MAGIC + struct.pack(">HHB3xI16x", w, h, fmt, size)


def _tiles(a, tw, th):
    """(H, W, C) -> (tilesY, tilesX, th, tw, C) in GX tile order."""
    h, w = a.shape[:2]
    return a.reshape(h // th, th, w // tw, tw, -1).transpose(0, 2, 1, 3, 4)


def _rgba(img, w, h):
    img = img.convert("RGBA")
    if img.size != (w, h): img = img.resize((w, h), Image.LANCZOS)
    return np.asarray(img, dtype=np.uint8)


def ia8(img, w=None, h=None):
    """Intensity + alpha, 4x4 tiles. Grey images become white text-style masks if alpha_from_luma."""
    w, h = w or img.size[0], h or img.size[1]
    a = _rgba(img, w, h).astype(np.uint16)
    lum = ((a[..., 0] * 77 + a[..., 1] * 150 + a[..., 2] * 29) >> 8).astype(np.uint8)
    out = np.stack([a[..., 3].astype(np.uint8), lum], -1)        # GX IA8 stores A then I
    return _tiles(out, 4, 4).tobytes()


def i8(img, w=None, h=None):
    w, h = w or img.size[0], h or img.size[1]
    g = np.asarray(img.convert("L").resize((w, h), Image.LANCZOS) if img.size != (w, h) else img.convert("L"), dtype=np.uint8)
    return _tiles(g[..., None], 8, 4).tobytes()


def rgb5a3(img, w=None, h=None):
    w, h = w or img.size[0], h or img.size[1]
    a = _rgba(img, w, h).astype(np.uint16)
    r, g, b, al = a[..., 0], a[..., 1], a[..., 2], a[..., 3]
    opaque = 0x8000 | ((r >> 3) << 10) | ((g >> 3) << 5) | (b >> 3)
    trans = ((al >> 5) << 12) | ((r >> 4) << 8) | ((g >> 4) << 4) | (b >> 4)
    v = np.where(al >= 0xE0, opaque, trans).astype(">u2")
    return _tiles(v[..., None], 4, 4).tobytes()


def _to565(c):
    c = np.clip(np.rint(c), 0, 255).astype(np.uint16)
    return ((c[..., 0] >> 3) << 11) | ((c[..., 1] >> 2) << 5) | (c[..., 2] >> 3)


def _from565(v):
    r = (v >> 11) & 31; g = (v >> 5) & 63; b = v & 31
    return np.stack([(r << 3) | (r >> 2), (g << 2) | (g >> 4), (b << 3) | (b >> 2)], -1).astype(np.float32)


def _dxt1_blocks(blocks, alpha=None):
    """blocks: (N, 16, 3) float. alpha: (N, 16) bool opaque mask or None. Returns (N, 8) uint8 GX-order blocks."""
    n = blocks.shape[0]
    mean = blocks.mean(1, keepdims=True)
    d = blocks - mean
    cov = np.einsum("nki,nkj->nij", d, d)
    axis = np.ones((n, 3), np.float32)
    for _ in range(6):                                         # power iteration for the principal axis
        axis = np.einsum("nij,nj->ni", cov, axis)
        axis /= np.maximum(np.linalg.norm(axis, axis=1, keepdims=True), 1e-6)
    proj = np.einsum("nki,ni->nk", d, axis)
    if alpha is not None: proj = np.where(alpha, proj, np.nan)
    lo = np.nan_to_num(np.nanmin(proj, 1), nan=0.0); hi = np.nan_to_num(np.nanmax(proj, 1), nan=0.0)
    c0 = mean[:, 0] + axis * hi[:, None]; c1 = mean[:, 0] + axis * lo[:, None]
    e0, e1 = _to565(c0), _to565(c1)
    three = np.zeros(n, bool) if alpha is None else ~alpha.all(1)
    # 4-colour mode needs e0 > e1, 3-colour (transparency) needs e0 <= e1
    swap = np.where(three, e0 > e1, e0 < e1)
    e0, e1 = np.where(swap, e1, e0), np.where(swap, e0, e1)
    eq = (e0 == e1) & ~three                                   # flat block: force 4-colour mode
    p0, p1 = _from565(e0), _from565(e1)
    pal4 = np.stack([p0, p1, (2 * p0 + p1) / 3, (p0 + 2 * p1) / 3], 1)
    pal3 = np.stack([p0, p1, (p0 + p1) / 2, np.full_like(p0, 1e9)], 1)
    pal = np.where(three[:, None, None], pal3, pal4)
    dist = ((blocks[:, :, None, :] - pal[:, None, :, :]) ** 2).sum(-1)     # (N, 16, 4)
    if alpha is not None: dist[:, :, 3] = np.where(three[:, None], 1e12, dist[:, :, 3])
    idx = dist.argmin(-1).astype(np.uint8)
    idx = np.where(eq[:, None], 0, idx)
    if alpha is not None: idx = np.where(alpha, idx, 3)
    rows = idx.reshape(n, 4, 4)
    packed = (rows[..., 0] << 6) | (rows[..., 1] << 4) | (rows[..., 2] << 2) | rows[..., 3]
    out = np.zeros((n, 8), np.uint8)
    out[:, 0] = e0 >> 8; out[:, 1] = e0 & 255; out[:, 2] = e1 >> 8; out[:, 3] = e1 & 255
    out[:, 4:] = packed
    return out


def cmpr_array(rgba):
    """(H, W, 4) uint8, H and W multiples of 8 -> CMPR bytes. Alpha < 128 becomes transparent."""
    h, w = rgba.shape[:2]
    a = rgba.astype(np.float32)
    # 8x8 tiles, then 2x2 sub-blocks of 4x4 pixels
    t = a.reshape(h // 8, 2, 4, w // 8, 2, 4, 4).transpose(0, 3, 1, 4, 2, 5, 6).reshape(-1, 16, 4)
    alpha = t[..., 3] >= 128
    has_alpha = not alpha.all()
    return _dxt1_blocks(t[..., :3], alpha if has_alpha else None).tobytes()


def cmpr(img, w=None, h=None):
    w, h = w or img.size[0], h or img.size[1]
    return cmpr_array(_rgba(img, w, h))


ENC = {CMPR: cmpr, RGB5A3: rgb5a3, IA8: ia8, I8: i8}


def save(path, img, fmt, w=None, h=None):
    w, h = w or img.size[0], h or img.size[1]
    data = ENC[fmt](img, w, h)
    with open(path, "wb") as f: f.write(header(w, h, fmt, len(data)) + data)
    return len(data) + 32


def decode_cmpr(data, w, h):
    """Reference decoder (tests and previews)."""
    blocks = np.frombuffer(data, np.uint8).reshape(-1, 8)
    e0 = blocks[:, 0].astype(np.uint16) << 8 | blocks[:, 1]; e1 = blocks[:, 2].astype(np.uint16) << 8 | blocks[:, 3]
    p0, p1 = _from565(e0), _from565(e1)
    four = e0 > e1
    pal = np.where(four[:, None, None], np.stack([p0, p1, (2 * p0 + p1) / 3, (p0 + 2 * p1) / 3], 1),
                   np.stack([p0, p1, (p0 + p1) / 2, np.zeros_like(p0)], 1))
    bits = blocks[:, 4:]
    idx = np.stack([(bits >> 6) & 3, (bits >> 4) & 3, (bits >> 2) & 3, bits & 3], -1).reshape(-1, 16)
    px = np.take_along_axis(pal, idx[..., None].astype(np.int64).repeat(3, -1), 1)
    img = px.reshape(h // 8, w // 8, 2, 2, 4, 4, 3).transpose(0, 2, 4, 1, 3, 5, 6).reshape(h, w, 3)
    return np.clip(img, 0, 255).astype(np.uint8)


if __name__ == "__main__":
    rng = np.random.default_rng(1)
    grad = np.zeros((16, 24, 4), np.uint8)
    grad[..., 0] = np.linspace(0, 255, 24)[None, :]; grad[..., 1] = np.linspace(0, 255, 16)[:, None]; grad[..., 2] = 90; grad[..., 3] = 255
    dec = decode_cmpr(cmpr_array(grad), 24, 16).astype(int)
    assert np.abs(dec - grad[..., :3]).mean() < 6, np.abs(dec - grad[..., :3]).mean()
    flat = np.full((8, 8, 4), (200, 30, 60, 255), np.uint8)
    assert np.abs(decode_cmpr(cmpr_array(flat), 8, 8).astype(int) - flat[..., :3]).max() < 10
    assert len(rgb5a3(Image.new("RGBA", (8, 8)), 8, 8)) == 128 and len(ia8(Image.new("RGBA", (8, 4)), 8, 4)) == 64
    print("gx OK")
