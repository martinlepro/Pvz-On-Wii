#!/usr/bin/env python3
"""Derive the HUD textures at the exact on-screen size (GRRLIB-safe: w/h multiples of 4).
Usage: python3 tools/make_hud_assets.py <assets_dir>
Writes into <assets_dir>/ui/: seedbar_bg.png, seed_frame.png, shovel_slot_small.png, coinbank_small.png
"""
import sys, os
from PIL import Image
A = sys.argv[1] if len(sys.argv) > 1 else "assets"
L = lambda p: Image.open(os.path.join(A, p)).convert("RGBA")
R = Image.LANCZOS

# 1. seed bar: 3-slice of ui/seedbank.png (446x87) -> 640x72
src = L("ui/seedbank.png"); s = 72 / src.height
capL, capR = 74, 10
left  = src.crop((0, 0, capL, src.height)).resize((round(capL * s), 72), R)
right = src.crop((src.width - capR, 0, src.width, src.height)).resize((round(capR * s), 72), R)
mid   = src.crop((120, 0, src.width - 40, src.height))
W = 640
bar = Image.new("RGBA", (W, 72), (0, 0, 0, 0))
midw = W - left.width - right.width
bar.paste(mid.resize((midw, 72), R), (left.width, 0))
bar.alpha_composite(left, (0, 0)); bar.alpha_composite(right, (W - right.width, 0))
bar.save(os.path.join(A, "ui/seedbar_bg.png"))

# 2. seed packet frame: blank card from other/seedpacket.png (104,0,50x72) -> 52x56
pk = L("other/seedpacket.png").crop((104, 0, 154, 72)).resize((52, 56), R)
pk.save(os.path.join(A, "ui/seed_frame.png"))

# 3. shovel slot 70x72 -> 56x56
L("ui/shovel_slot.png").resize((56, 56), R).save(os.path.join(A, "ui/shovel_slot_small.png"))

# 4. coin bank 128x31 -> 104x24
L("ui/coinbank.png").resize((104, 24), R).save(os.path.join(A, "ui/coinbank_small.png"))
print("ok")
