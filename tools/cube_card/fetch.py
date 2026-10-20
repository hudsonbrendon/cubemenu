"""Download what the disc does not carry, into <sd>/cubemenu/src/<GAMEID>/:

  coverfull.png  GameTDB full cover (back | spine | front)     https://art.gametdb.com
  disc.png       GameTDB disc label (RGBA)
  snap.png       libretro thumbnail, gameplay screenshot
  title.png      libretro thumbnail, title screen
  synopsis.<pt|en|es>.txt   Wikipedia lead section
  video.mp4      gameplay clip (yt-dlp, trimmed to 20 s, no audio)
  music.mp3      theme / OST (yt-dlp, 60 s)
"""
import difflib, json, os, re, shutil, subprocess, tempfile, urllib.parse, urllib.request

UA = {"User-Agent": "cubemenu/0.1 (https://github.com/hudsonbrendon/cubemenu)"}
TDB = "https://art.gametdb.com/wii/"
LIBRETRO = "https://thumbnails.libretro.com/Nintendo%20-%20GameCube/"
LANGS = ("pt", "en", "es")
TDB_REGIONS = {"E": ("US", "EN"), "P": ("EN", "US"), "J": ("JA", "US"), "D": ("DE", "EN"), "F": ("FR", "EN"),
               "S": ("ES", "EN"), "I": ("IT", "EN")}
LIBRETRO_REGION = {"E": "USA", "P": "Europe", "J": "Japan"}


def get(url, timeout=60):
    return urllib.request.urlopen(urllib.request.Request(url, headers=UA), timeout=timeout).read()


def _save_png(url, dst):
    try: data = get(url)
    except Exception: return False
    if data[:8] != b"\x89PNG\r\n\x1a\n": return False
    with open(dst, "wb") as f: f.write(data)
    return True


def fetch_tdb(g, src, force=False, log=print):
    """coverfull (HQ first) and disc art from GameTDB, trying the game's own region first."""
    regions = TDB_REGIONS.get(g["id"][3], ("US", "EN"))
    for name, kinds in (("coverfull.png", ("coverfullHQ", "coverfull")), ("disc.png", ("disc",))):
        dst = os.path.join(src, name)
        if os.path.exists(dst) and not force: continue
        ok = any(_save_png(f"{TDB}{k}/{r}/{g['id']}.png", dst) for k in kinds for r in regions)
        log(f"  {'ok' if ok else '??'} {g['id']} {g['name']}: {name}")


def _norm(s):
    s = re.sub(r"\.png$", "", s, flags=re.I).lower().replace("&", "and")
    s = re.sub(r"\s*\(.*?\)|\s*\[.*?\]", "", s)
    s = re.sub(r"\b(disc|disco)\s*\d\b", " ", s)
    return re.sub(r"[^a-z0-9]+", " ", s).strip()


def _digits(s): return re.findall(r"\d+", s)


def match(title, region, names):
    """Best libretro file for a title (numbers must agree, the game's own region preferred)."""
    want = _norm(title)
    by = {}
    for n in names: by.setdefault(_norm(n), []).append(n)
    keys = [k for k in by if _digits(k) == _digits(want)]
    hit = want if want in by else None
    if not hit:
        pre = [k for k in keys if k.startswith(want + " ") or want.startswith(k + " ")]
        if pre: hit = min(pre, key=len)
    if not hit:
        close = difflib.get_close_matches(want, keys, n=1, cutoff=0.82)
        hit = close[0] if close else None
    if not hit: return None
    cands = sorted(by[hit], key=len)
    for c in cands:
        if "(" + LIBRETRO_REGION.get(region, "USA") in c: return c
    return cands[0]


_idx = {}
def _libretro_index(kind):
    if kind not in _idx:
        html = get(LIBRETRO + kind + "/", 60).decode("utf-8", "replace")
        _idx[kind] = [urllib.parse.unquote(h) for h in re.findall(r'href="([^"?]+\.png)"', html)]
    return _idx[kind]


def fetch_shots(g, src, force=False, log=print):
    for kind, name in (("Named_Snaps", "snap.png"), ("Named_Titles", "title.png")):
        dst = os.path.join(src, name)
        if os.path.exists(dst) and not force: continue
        try: names = _libretro_index(kind)
        except Exception as e: log(f"  !! libretro {kind}: {e}"); continue
        fn = match(g["name"], g["id"][3], names) or match(g["title"], g["id"][3], names)
        ok = bool(fn) and _save_png(LIBRETRO + kind + "/" + urllib.parse.quote(fn), dst)
        log(f"  {'ok' if ok else '??'} {g['id']} {g['name']}: {name} {fn or ''}")


# ---------- synopses ----------
def _wiki(lang, params):
    url = f"https://{lang}.wikipedia.org/w/api.php?" + urllib.parse.urlencode(dict(params, format="json"))
    for attempt in range(3):
        try: return json.loads(get(url, 30))
        except Exception:
            if attempt == 2: raise


def wiki_article(title, lang):
    """Title of the article about the game (digits must agree); None if unsure. An article whose title
    is shorter than the game's ("The Legend of Zelda" for "... The Wind Waker") is the series, not the game."""
    want = _norm(title)
    res = _wiki(lang, {"action": "query", "list": "search", "srsearch": f"{title} GameCube", "srlimit": 8})["query"]["search"]
    best = None
    for r in res:
        n = _norm(r["title"])
        if _digits(n) != _digits(want) or (want.startswith(n + " ") and n != want): continue
        score = difflib.SequenceMatcher(None, want, n).ratio()
        if n == want or n.startswith(want + " ") and score > 0.6: score += 0.5
        if re.search(r"\((video ?game|jogo eletr[oô]nico|videojuego)", r["title"], re.I): score += 0.1
        if score >= 0.8 and (not best or score > best[0]): best = (score, r["title"])
    return best[1] if best else None


def wiki_langlink(article, src_lang, lang):
    """The same article in another Wikipedia."""
    pages = _wiki(src_lang, {"action": "query", "prop": "langlinks", "lllang": lang, "redirects": 1, "titles": article})["query"]["pages"]
    ll = list(pages.values())[0].get("langlinks") or []
    return ll[0]["*"] if ll else None


def wiki_synopsis(title, lang, article=None):
    """Lead section of the game's article; None if unsure."""
    article = article or wiki_article(title, lang)
    if not article: return None
    pages = _wiki(lang, {"action": "query", "prop": "extracts", "exintro": 1, "explaintext": 1, "redirects": 1, "titles": article})["query"]["pages"]
    txt = list(pages.values())[0].get("extract", "").strip()
    if len(txt) < 80 or not re.search(r"gamecube|jogo|game|videojuego|videogame|juego", txt, re.I): return None
    txt = re.sub(r"\s*\([^()]*?[ぁ-んァ-ン一-龯][^()]*\)", "", txt)
    return re.sub(r"[ \t]+", " ", txt)[:3000]


def fetch_synopses(g, src, force=False, log=print):
    en = None          # the English article, whose language links give the same article elsewhere
    titles = [t for t in (g.get("tdb_title"), g["name"], g["title"]) if t]
    for t in titles:
        try: en = wiki_article(t, "en")
        except Exception as e: log(f"  !! wikipedia en: {e}")
        if en: break
    for lang in LANGS:
        dst = os.path.join(src, f"synopsis.{lang}.txt")
        if os.path.exists(dst) and not force: continue
        txt = None
        try:
            art = en if lang == "en" else wiki_langlink(en, "en", lang) if en else None
            if art: txt = wiki_synopsis(None, lang, art)
        except Exception as e: log(f"  !! wikipedia {lang}: {e}")
        for t in (() if txt else titles):
            try: txt = wiki_synopsis(t, lang)
            except Exception as e: log(f"  !! wikipedia {lang}: {e}")
            if txt: break
        if not txt and force and os.path.exists(dst): os.remove(dst)    # a wrong old one is worse than none
        if txt:
            with open(dst, "w", encoding="utf-8") as f: f.write(txt)
        log(f"  {'ok' if txt else '??'} {g['id']} {g['name']}: synopsis.{lang}")


# ---------- clips and music (yt-dlp) ----------
def _query(g): return re.sub(r"\s*\((v1\.\d+|player's choice)\)", "", g["name"], flags=re.I)


def _duration(path):
    r = subprocess.run(["ffprobe", "-v", "error", "-show_entries", "format=duration", "-of", "csv=p=0", path], capture_output=True, text=True)
    try: return float(r.stdout.strip())
    except ValueError: return 0.0


def _found(src, stem):
    for f in os.listdir(src):
        if f.startswith(stem + ".") and not f.endswith((".part", ".ytdl", ".txt")): return os.path.join(src, f)
    return None


def _ytdlp(search, out, fmt, extra):
    cmd = ["yt-dlp", "-q", "--no-warnings", "--no-playlist", "--extractor-args", "youtube:player_client=android", search, "--max-downloads", "1", "-f", fmt,
           "--force-overwrites", "-o", out] + extra
    return subprocess.run(cmd, capture_output=True, text=True, timeout=900)


def fetch_video(g, src, force=False, log=print):
    dst = os.path.join(src, "video.mp4")
    if os.path.exists(dst) and not force: return
    out = os.path.join(src, "dl_video.%(ext)s")
    for search, maxdur in ((f"ytsearch6:{_query(g)} gamecube gameplay", 900), (f"ytsearch10:{_query(g)} gameplay", 2400),
                           (f"ytsearch8:{_query(g)} gamecube trailer", 600)):
        r = _ytdlp(search, out, "bv*[height<=480][ext=mp4]/bv*[height<=480]/b[height<=480]/b",
                   ["--match-filter", f"duration<{maxdur} & duration>60"])
        got = _found(src, "dl_video")
        if got: break
    if not got: log(f"  ?? {g['id']} {g['name']}: no clip {(r.stderr or '').strip()[-120:]}"); return
    dur = _duration(got)
    start = max(0.0, min(dur * 0.30, dur - 20)) if dur > 30 else 0.0
    rr = subprocess.run(["ffmpeg", "-v", "error", "-y", "-ss", f"{start:.1f}", "-i", got, "-t", "20", "-an",
                         "-vf", "scale=-2:360", "-c:v", "libx264", "-crf", "23", "-preset", "veryfast", dst], capture_output=True)
    os.remove(got)
    log(f"  {'ok' if rr.returncode == 0 else '!!'} {g['id']} {g['name']}: clip")


def fetch_music(g, src, force=False, log=print):
    dst = os.path.join(src, "music.mp3")
    if os.path.exists(dst) and not force: return
    out = os.path.join(src, "dl_music.%(ext)s")
    for search in (f"ytsearch6:{_query(g)} gamecube ost main theme", f"ytsearch8:{_query(g)} ost title theme"):
        r = _ytdlp(search, out, "bestaudio/best",
                   ["--match-filter", "duration<420 & duration>45", "-x", "--audio-format", "mp3", "--audio-quality", "5"])
        got = _found(src, "dl_music")
        if got: break
    if not got: log(f"  ?? {g['id']} {g['name']}: no music {(r.stderr or '').strip()[-120:]}"); return
    rr = subprocess.run(["ffmpeg", "-v", "error", "-y", "-i", got, "-t", "60", "-ac", "2", "-ar", "32000", "-b:a", "96k",
                         "-af", "afade=t=out:st=57:d=3", dst], capture_output=True)
    os.remove(got)
    log(f"  {'ok' if rr.returncode == 0 else '!!'} {g['id']} {g['name']}: music")


if __name__ == "__main__":
    names = ["Super Smash Bros. Melee (USA).png", "Super Smash Bros. Melee (Europe).png", "Metroid Prime (USA).png",
             "Metroid Prime 2 - Echoes (USA).png", "Pikmin (USA).png", "Pikmin 2 (USA).png"]
    assert match("Super Smash Bros. Melee (v1.01)", "E", names) == "Super Smash Bros. Melee (USA).png"
    assert match("Super Smash Bros. Melee", "P", names) == "Super Smash Bros. Melee (Europe).png"
    assert match("Metroid Prime", "E", names) == "Metroid Prime (USA).png"
    assert match("Metroid Prime 2 - Echoes", "E", names) == "Metroid Prime 2 - Echoes (USA).png"
    assert match("Pikmin", "E", names) == "Pikmin (USA).png"
    print("fetch OK")
