#!/usr/bin/env python3
"""Derive title/minigame textures at on-screen size (GRRLIB-safe, w/h multiples of 4).
Usage: python3 tools/make_screen_assets.py <assets_dir>
Writes <assets_dir>/ui/: title_art.png (640x480), title_logo.png, mg_*.png
"""
import sys, os
import numpy as np
from PIL import Image
A = sys.argv[1] if len(sys.argv) > 1 else "assets"
P = lambda p: os.path.join(A, p)
R = Image.LANCZOS
CY = np.array([0, 148, 255])

sheet = Image.open(P("ui/PC _ Computer - Plants vs. Zombies - UI - Title Screen.png")).convert("RGBA")
# key art: 800x600 block at (5,5); logo block y 610..726
art = sheet.crop((5, 5, 805, 605)).convert("RGB").resize((640, 480), R)
art.save(P("ui/title_art.png"))

logo = np.array(sheet.crop((0, 610, 810, 726)).convert("RGB")).astype(float)
# The logo block sits on a solid black rectangle: key it out, then rebuild a
# clean black outline around the letters so it reads on any background.
from scipy import ndimage
lum = logo.max(axis=2)
notcy = np.abs(logo - CY).sum(axis=2) > 90          # drop the sheet's cyan backdrop
mask = ndimage.binary_opening((lum > 45) & notcy, iterations=1)
region = ndimage.binary_fill_holes(ndimage.binary_dilation(mask, iterations=3))
alpha = ndimage.gaussian_filter(region.astype(float), 0.8)
rgb = np.where(mask[:, :, None], logo, 0.0)
rgba = np.dstack([rgb, alpha * 255]).astype(np.uint8)
img = Image.fromarray(rgba, "RGBA")
img = img.crop(img.getbbox())
w = 420; h0 = round(img.height * w / img.width); h = h0 + (-h0) % 4
img = img.resize((w, h0), R)
canvas = Image.new("RGBA", (w, h), (0, 0, 0, 0)); canvas.paste(img, (0, 0))
canvas.save(P("ui/title_logo.png"))

# minigame icons from other/minigames.png (80x65 tiles, row-major; first 20 = mini-games)
mg = Image.open(P("other/minigames.png")).convert("RGBA")
rows = [29, 99, 169, 239]; cols = [6, 91, 176, 261, 346]
tiles = [(r, c) for r in rows for c in cols]
# ADJUST HERE if an icon is not the one you expect (tile index, row-major)
ICONS = {"big_trouble": 9, "nimble_quick": 13, "column_like": 11,
         "invisi_ghoul": 16, "last_stand": 15}
for name, i in ICONS.items():
    r, c = tiles[i]
    t = mg.crop((c, r, c + 80, r + 65))
    out = Image.new("RGBA", (80, 68), (0, 0, 0, 0)); out.paste(t, (0, 0))
    out.save(P("ui/mg_%s.png" % name))
print("ok")
