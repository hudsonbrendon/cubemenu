"""Generate everything under data/ that the menu embeds (fonts, logo, console and UI textures)."""
import os, subprocess, sys
HERE = os.path.dirname(os.path.abspath(__file__))
for script in ("mkfont.py", "mkbrand.py", "mkconsole.py"):
    p = os.path.join(HERE, script)
    if os.path.exists(p): subprocess.run([sys.executable, p], check=True)
