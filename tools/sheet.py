"""Contact sheet of Dolphin frame dumps: python3 tools/sheet.py out.png N1 N2 ... (frame numbers)"""
import sys, os
from PIL import Image
D = os.path.join(os.path.dirname(__file__), "..", ".dolphin", "Dump", "Frames")
out, nums = sys.argv[1], [int(x) for x in sys.argv[2:]]
W, H = 400, 300
cols = 3
sheet = Image.new("RGB", (W * cols, H * ((len(nums) + cols - 1) // cols)))
for k, n in enumerate(nums):
    im = Image.open(os.path.join(D, f"framedump_{n}.png")).convert("RGB").resize((W, H))
    sheet.paste(im, ((k % cols) * W, (k // cols) * H))
sheet.save(out)
