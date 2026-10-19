#!/usr/bin/env python3
"""Derive Zen Garden textures (GRRLIB-safe sizes).
Usage: python3 tools/make_zen_assets.py <assets_dir>
Writes <assets_dir>/ui/: zen_garden_main.png, zen_garden_mushroom.png (256x192),
zen_reticle.png (196x196), zen_wheelbarrow.png (84x84)
"""
import sys, os
from PIL import Image
A = sys.argv[1] if len(sys.argv) > 1 else "assets"
P = lambda p: os.path.join(A, p)
sheet = Image.open(P("background/zen garden.png")).convert("RGB")
# the sheet stacks 4 gardens (256x192 each, 8px labels); we use the first two
sheet.crop((1, 8, 257, 200)).save(P("ui/zen_garden_main.png"))
sheet.crop((1, 207, 257, 399)).save(P("ui/zen_garden_mushroom.png"))

def pad4(src, dst):
    im = Image.open(P(src)).convert("RGBA")
    w, h = im.size; nw, nh = w + (-w) % 4, h + (-h) % 4
    out = Image.new("RGBA", (nw, nh), (0, 0, 0, 0)); out.paste(im, (0, 0)); out.save(P(dst))
pad4("ui/zen_GoldToolReticle.png", "ui/zen_reticle.png")
pad4("ui/zen_WheelBarrow.png", "ui/zen_wheelbarrow.png")
print("ok")
