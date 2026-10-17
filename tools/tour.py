"""Scripted tour in Dolphin: screenshots for the README and the demo video (with sound).

  python3 tools/tour.py <sd_root> [--shots] [--video] [--lang 1]

Builds a test disc whose /cubemenu/demo.txt drives the menu (the menu injects the buttons on the
listed frames), runs Dolphin with frame + audio dumping, then picks frames by number. Frame N of the
dump is app frame N, so screenshots land exactly where the script says.
"""
import argparse, glob, os, shutil, subprocess, sys, time, wave
sys.path.insert(0, os.path.join(os.path.dirname(__file__), ".."))

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), ".."))
DOLPHIN = "/Applications/Dolphin.app/Contents/MacOS/Dolphin"
UDIR = os.path.join(ROOT, ".dolphin")


class Script:
    def __init__(self):
        self.events, self.frame, self.shots, self.last = [], 0, [], 0

    def wait(self, n): self.frame += n; return self

    def press(self, button, then=0):
        self.events.append((self.frame - self.last, button))
        self.last = self.frame
        self.frame += then
        return self

    def shot(self, name, back=0): self.shots.append((self.frame - back, name)); return self

    def text(self): return "".join(f"{w} {b}\n" for w, b in self.events)


def tour_script(view_themes=True):
    s = Script()
    s.wait(120).shot("intro").wait(110)                       # intro ends ~192
    s.wait(150).shot("coverflow")                            # first game, clip + music running
    s.press("RIGHT", 150).shot("coverflow-2")
    s.press("RIGHT", 150).shot("coverflow-3")
    s.press("Y", 140).shot("details")
    s.press("DOWN", 12).press("DOWN", 12).press("DOWN", 60).shot("details-scroll")
    s.press("R", 140).shot("details-2")
    s.press("B", 40)
    s.press("B", 50).shot("recent").press("B", 30)
    # views: settings > view (row 3)
    s.press("X", 40).shot("settings").press("DOWN", 8).press("DOWN", 8).press("RIGHT", 10).press("B", 120).shot("grid")
    s.press("RIGHT", 30).press("DOWN", 120).shot("grid-2")
    s.press("X", 30).press("DOWN", 8).press("DOWN", 8).press("RIGHT", 10).press("B", 140).shot("list")
    s.press("X", 30).press("DOWN", 8).press("DOWN", 8).press("RIGHT", 10).press("B", 60)
    s.press("START", 90).shot("system").press("B", 40)
    if view_themes:
        for name in ("theme-jet-black", "theme-platinum", "theme-spice", "theme-emerald", "theme-gold"):
            s.press("X", 30).press("RIGHT", 20).press("B", 70).press("RIGHT", 140).shot(name)
        s.press("X", 30).press("RIGHT", 20).press("B", 60)      # back to Indigo
    s.press("LEFT", 120)
    s.press("A", 40).shot("launch-1", 0).wait(40).shot("launch-2").wait(40).shot("launch-3").wait(45).shot("launch-4")
    s.wait(60).shot("launch-5").wait(120)
    return s


def video_script():
    """~70 s: intro, browsing with clips and themes, details, views, the launch."""
    s = Script()
    s.wait(230)
    s.wait(180)
    for _ in range(4): s.press("RIGHT", 170)
    s.press("Y", 150).press("DOWN", 20).press("DOWN", 20).press("DOWN", 90)
    s.press("R", 150).press("B", 60)
    s.press("RIGHT", 40).press("RIGHT", 160)
    s.press("X", 50).press("RIGHT", 40).press("B", 80).press("RIGHT", 150)      # Jet Black
    s.press("X", 40).press("RIGHT", 30).press("RIGHT", 30).press("B", 80).press("RIGHT", 150)   # Spice
    s.press("X", 40).press("LEFT", 30).press("LEFT", 30).press("LEFT", 30).press("B", 60)        # Indigo
    s.press("X", 40).press("DOWN", 15).press("DOWN", 15).press("RIGHT", 30).press("B", 110)    # grid
    s.press("RIGHT", 30).press("DOWN", 90)
    s.press("X", 40).press("DOWN", 15).press("DOWN", 15).press("RIGHT", 30).press("B", 110)    # list
    s.press("DOWN", 30).press("DOWN", 60)
    s.press("X", 40).press("DOWN", 15).press("DOWN", 15).press("RIGHT", 30).press("B", 90)     # back to shelf
    s.press("START", 120).press("B", 50)
    s.press("A", 330)
    return s


def run(sd, script, seconds_cap, ir, games=None, settings=None, stats=None, lang=1, start="GM8E01", speed=1.0):
    os.makedirs(os.path.join(ROOT, "tour"), exist_ok=True)
    demo = os.path.join(ROOT, "tour", "demo.txt")
    open(demo, "w").write(script.text())
    st = os.path.join(ROOT, "tour", "settings.ini")
    open(st, "w").write(settings or f"theme=0\nlanguage={lang}\nview=0\nlast_game={start}\nscreensaver=0\n")
    cmd = [sys.executable, os.path.join(ROOT, "tools", "mktestdisc.py"), sd, os.path.join(ROOT, "dist", "test.iso"),
           "--demo", demo, "--settings", st]
    if stats: cmd += ["--stats", stats]
    if games: cmd += ["--only", games]
    subprocess.run(cmd, check=True)
    with open(os.path.join(UDIR, "Config", "GFX.ini"), "w") as f:
        f.write(f"[Settings]\nInternalResolution = {ir}\nInternalResolutionFrameDumps = True\n")
    ini = os.path.join(UDIR, "Config", "Dolphin.ini")
    lines = [l for l in open(ini).read().splitlines() if not l.startswith("EmulationSpeed")]
    lines.insert(lines.index("[Core]") + 1, f"EmulationSpeed = {speed}")   # slower = the PNG dumper drops no frames
    open(ini, "w").write("\n".join(lines) + "\n")
    for d in ("Dump/Frames", "Dump/Audio"): shutil.rmtree(os.path.join(UDIR, d), ignore_errors=True)
    p = subprocess.Popen([DOLPHIN, "-u", UDIR, "-b", "-e", os.path.join(ROOT, "dist", "test.iso")],
                         stdout=subprocess.DEVNULL, stderr=subprocess.DEVNULL)
    target = script.frame + 10
    t0 = time.time()
    last_n, last_change = 0, time.time()
    while time.time() - t0 < seconds_cap:
        n = len(glob.glob(os.path.join(UDIR, "Dump", "Frames", "framedump_*.png")))
        if n >= target: break
        if p.poll() is not None: break
        if n != last_n: last_n, last_change = n, time.time()
        elif time.time() - last_change > 25 / speed: break          # handed over to Swiss (draws nothing in Dolphin)
        time.sleep(3)
    p.terminate()
    try: p.wait(10)
    except subprocess.TimeoutExpired: p.kill()
    n = len(glob.glob(os.path.join(UDIR, "Dump", "Frames", "framedump_*.png")))
    print(f"dumped {n} frames (wanted {target}) in {time.time() - t0:.0f}s")
    return n


def frame_path(n): return os.path.join(UDIR, "Dump", "Frames", f"framedump_{n}.png")


def read_code(img):
    """App frame number from the 20-bit strip the menu draws in demo mode (bottom 4 rows)."""
    w, h = img.size
    sx, y = w / 640, h - max(1, round(2 * h / 480))
    v = 0
    for b in range(20):
        px = img.getpixel((int((b * 32 + 16) * sx), y))
        if sum(px[:3]) > 384: v |= 1 << b
    return v


def index_frames():
    """{app frame: dump file} for every dumped frame."""
    from PIL import Image
    out = {}
    for f in glob.glob(os.path.join(UDIR, "Dump", "Frames", "framedump_*.png")):
        out.setdefault(read_code(Image.open(f).convert("RGB")), f)
    return out


def clean(img):
    """Paint the code strip over with the row above it."""
    w, h = img.size
    k = max(1, round(4 * h / 480))
    row = img.crop((0, h - k - 1, w, h - k))
    for y in range(h - k, h): img.paste(row, (0, y))
    return img


def save_shots(script, out_dir):
    from PIL import Image
    os.makedirs(out_dir, exist_ok=True)
    idx = index_frames()
    for frame, name in script.shots:
        near = [k for k in idx if abs(k - frame) <= 6]
        if not near: print("  missing", name, frame); continue
        src = idx[min(near, key=lambda k: abs(k - frame))]
        clean(Image.open(src).convert("RGB")).save(os.path.join(out_dir, name + ".png"), optimize=True)
        print("  ", name)


CHIME_FRAME = 70     # app frame of the intro chime (screen_t > 1.15 s)


def chime_time(wav_path):
    """Seconds into the DSP dump where the intro chime starts (first loud onset)."""
    import numpy as np
    with wave.open(wav_path) as w:
        rate, ch = w.getframerate(), w.getnchannels()
        a = np.frombuffer(w.readframes(w.getnframes()), np.int16).reshape(-1, ch).astype(np.float32).mean(1)
    win = rate // 100
    env = np.abs(a[: len(a) // win * win]).reshape(-1, win).max(1)
    hit = np.argmax(env > 2500)
    return hit * win / rate


def make_video(out, last_frame):
    """Rebuild a constant 60 fps sequence from the coded frames (repeating the previous one where the
    dumper dropped a frame), then mux the DSP audio aligned on the intro chime."""
    from PIL import Image
    idx = index_frames()
    seq = os.path.join(UDIR, "Dump", "Seq")
    shutil.rmtree(seq, ignore_errors=True); os.makedirs(seq)
    prev = None
    first = min(idx)
    for k in range(first, last_frame + 1):
        if k in idx: prev = idx[k]
        if prev: os.symlink(prev, os.path.join(seq, f"{k - first:06d}.png"))
    wavs = sorted(glob.glob(os.path.join(UDIR, "Dump", "Audio", "*dspdump*.wav")), key=os.path.getsize)
    audio = wavs[-1] if wavs else None
    raw = out + ".raw.mp4"
    subprocess.run(["ffmpeg", "-v", "error", "-y", "-framerate", "60", "-i", os.path.join(seq, "%06d.png"),
                    "-vf", "crop=iw:ih-ih/120:0:0,scale=1280:960:flags=lanczos,format=yuv420p", "-c:v", "libx264", "-crf", "18",
                    "-preset", "slow", raw], check=True)
    cmd = ["ffmpeg", "-v", "error", "-y", "-i", raw]
    if audio:
        offset = chime_time(audio) - (CHIME_FRAME - first) / 60.0
        print(f"audio offset {offset:.3f}s")
        cmd += ["-ss", f"{max(0, offset):.3f}", "-i", audio, "-map", "0:v", "-map", "1:a", "-shortest",
                "-c:a", "aac", "-b:a", "160k", "-af", "loudnorm=I=-16:TP=-1.5"]
    cmd += ["-c:v", "copy", "-movflags", "+faststart", out]
    subprocess.run(cmd, check=True)
    os.remove(raw)
    print("video:", out, f"{(last_frame - first) / 60:.1f}s")


if __name__ == "__main__":
    ap = argparse.ArgumentParser()
    ap.add_argument("sd"); ap.add_argument("--shots", action="store_true"); ap.add_argument("--video", action="store_true")
    ap.add_argument("--lang", type=int, default=1); ap.add_argument("--stats"); ap.add_argument("--ir", type=int, default=2)
    a = ap.parse_args()
    if a.shots:
        s = tour_script()
        run(a.sd, s, 3600, a.ir, stats=a.stats, lang=a.lang)
        save_shots(s, os.path.join(ROOT, "docs", "screenshots"))
    if a.video:
        s = video_script()
        n = run(a.sd, s, 9000, a.ir, stats=a.stats, lang=a.lang, start="GFZE01", speed=0.4)
        make_video(os.path.join(ROOT, "dist", "cubemenu-demo.mp4"), s.frame)
