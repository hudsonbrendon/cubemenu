"""Build a bootable GameCube disc image: GC Loader boots it as /boot.iso, Dolphin boots it as a disc.

  python3 tools/mkiso.py dist/cubemenu.dol dist/boot.iso [--files DIR]

Layout: disc header + bi2 + our apploader in the 32 KB ISO9660 system area, an ISO9660 file system
(optional --files: the emulator test library), then the DOL appended at the end. The header's
0x420 field points the apploader at the DOL.
"""
import argparse, os, shutil, struct, subprocess, sys, tempfile

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
GAME_ID = b"GCMEHB"          # homebrew, not a retail ID ("E" = NTSC-U for emulators)
TITLE = b"Cube Menu"


def system_area(apploader, region=3):
    sa = bytearray(0x8000)
    sa[0:6] = GAME_ID
    sa[0x1C:0x20] = b"\xc2\x33\x9f\x3d"
    sa[0x20:0x20 + len(TITLE)] = TITLE
    fst_off, fst = 0x7000, struct.pack(">BBHII", 1, 0, 0, 0, 1) + b"\0" * 4
    struct.pack_into(">III", sa, 0x424, fst_off, len(fst), len(fst))
    sa[fst_off:fst_off + len(fst)] = fst
    struct.pack_into(">I", sa, 0x444, 0x01800000)      # bi2: simulated memory size
    struct.pack_into(">I", sa, 0x458, region)          # bi2: region (3 = any, 1 = NTSC-U for the emulator)
    struct.pack_into(">I", sa, 0x45C, 1)
    if len(apploader) > fst_off - 0x2460: sys.exit("apploader too big")
    sa[0x2440:0x2450] = b"2026/10/17".ljust(16, b"\0")
    struct.pack_into(">III", sa, 0x2450, 0x81200000, len(apploader), 0)
    sa[0x2460:0x2460 + len(apploader)] = apploader
    return bytes(sa)


def build(dol, out, files=None, apploader=None, region=3):
    apploader = open(apploader or os.path.join(ROOT, "apploader", "apploader.bin"), "rb").read()
    with tempfile.TemporaryDirectory() as tmp:
        sa = os.path.join(tmp, "sa.bin")
        open(sa, "wb").write(system_area(apploader, region))
        src = files
        if not src:
            src = os.path.join(tmp, "root"); os.makedirs(src)
            open(os.path.join(src, "README.TXT"), "w").write("Cube Menu boot disc - https://github.com/hudsonbrendon/cubemenu\n")
        mk = shutil.which("mkisofs") or shutil.which("genisoimage") or shutil.which("xorrisofs")
        r = subprocess.run([mk, "-quiet", "-G", sa, "-J", "-joliet-long", "-R", "-V", "CUBEMENU", "-o", out + ".tmp", src],
                           capture_output=True, text=True)
        if r.returncode: sys.exit(r.stderr)
    data = bytearray(open(out + ".tmp", "rb").read())
    os.remove(out + ".tmp")
    if data[0x1C:0x20] != b"\xc2\x33\x9f\x3d": sys.exit("system area was not kept by mkisofs")
    dol_off = (len(data) + 0x7FFF) & ~0x7FFF
    data += b"\0" * (dol_off - len(data))
    data += open(dol, "rb").read()
    data += b"\0" * (-len(data) % 0x8000)
    struct.pack_into(">I", data, 0x420, dol_off)
    open(out, "wb").write(data)
    return dol_off, len(data)


if __name__ == "__main__":
    p = argparse.ArgumentParser()
    p.add_argument("dol"); p.add_argument("out"); p.add_argument("--files")
    a = p.parse_args()
    off, size = build(a.dol, a.out, a.files)
    print(f"{a.out}: {size // 1024} KB, DOL at 0x{off:x}")
