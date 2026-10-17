#!/usr/bin/env python3
"""Pad every PNG under a folder so width and height are multiples of 4.

Why: GRRLIB's PNG loader (PNGU_DecodeTo4x4RGBA8) rounds the size up to a
multiple of 4 for the Wii GPU, and when it has to do that GRRLIB wipes the
whole texture to transparent. Result: the file "loads" fine (no missing-texture
flag) but draws nothing. Padding the files ahead of time avoids that.

Usage: python3 pad_textures.py <assets_dir>      (edits files in place)
- Sprites: transparent padding added at the TOP (feet stay on the bottom edge)
  and split left/right (horizontal centre stays put).
- Opaque images (backgrounds, menu): edge pixels are repeated instead.
"""
import sys, os
from PIL import Image

def pad(path):
    im = Image.open(path)
    w, h = im.size
    if w % 4 == 0 and h % 4 == 0:
        return False
    nw, nh = (w + 3) & ~3, (h + 3) & ~3
    opaque = im.mode == "RGB" or (im.mode == "RGBA" and im.getchannel("A").getextrema()[0] == 255)
    im = im.convert("RGBA")
    dx, dy = (nw - w) // 2, nh - h
    out = Image.new("RGBA", (nw, nh), (0, 0, 0, 0))
    out.paste(im, (dx, dy))
    if opaque:  # replicate edges so there is no transparent seam
        px = out.load()
        for y in range(nh):
            for x in range(nw):
                sx = min(max(x - dx, 0), w - 1)
                sy = min(max(y - dy, 0), h - 1)
                if x < dx or x >= dx + w or y < dy or y >= dy + h:
                    px[x, y] = im.getpixel((sx, sy))
    out.save(path, optimize=False)
    return True

root = sys.argv[1]
n = 0
for d, _, fs in os.walk(root):
    for f in fs:
        if f.lower().endswith(".png") and pad(os.path.join(d, f)):
            n += 1
print("padded", n, "files")
