# Disc Assets (static)

Place **optional** files here that should appear on the Wii disc (not the Homebrew Channel bundle).

## Files

| File | Description |
|------|-------------|
| `opening.bnr` | Disc banner (optional). Generate with `python scripts/generate_opening_bnr.py`. |
| `files/` | Extra data copied to `disc/staging/files/` (e.g. large assets, levels). |

## Homebrew vs disc output

| Output | Location | Use case |
|--------|----------|----------|
| Homebrew `.dol` | `output/homebrew/boot.dol` | SD card / Homebrew Channel |
| Disc image | `output/disc/pvz_wii.iso` | Dolphin, USB loaders |
| WBFS | `output/disc/pvz_wii.wbfs` | Compressed Wii backup format |

The root `pvz_wii.dol` is still produced for quick devkitPro workflows.
