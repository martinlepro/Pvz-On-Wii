# Asset manifest

No art is bundled yet — every sprite below is loaded **at runtime** from a
plain relative path (the same way `assets/font.ttf` already worked), and
`render.cpp` gracefully falls back to a flat colored rectangle for anything
that isn't present. That means:

- You can drop files in one at a time; nothing else needs to change.
- Wrong dimensions won't break anything — everything is scaled to fit its
  on-screen box automatically. The sizes below are *recommended* (for
  crispness / texture-memory reasons on real Wii hardware), not mandatory.
- All files are plain PNG with transparency (RGBA). Use a transparent
  background unless noted otherwise.

Run `make` (or `make homebrew` / `make iso`) again after adding files — the
build already copies the whole `assets/` folder into both the Homebrew
Channel bundle and the disc image for you (no Makefile changes needed).

## Where things go

```
assets/
├── font.ttf                     (already supported — HUD/menu text)
├── seeds/                       48×48 px — icon shown in the seed-bar slot
│   ├── peashooter.png
│   ├── sunflower.png
│   ├── wallnut.png
│   ├── cherrybomb.png
│   ├── repeater.png
│   └── snowpea.png
├── plants/                      recommended 64×80 px — sprite on the lawn
│   ├── peashooter.png            ✅ provided (background-removed cutout)
│   ├── sunflower.png             ✅ provided
│   ├── wallnut.png                ✅ provided
│   ├── cherrybomb.png            ✅ provided
│   ├── repeater.png               not provided yet — falls back to a flat box
│   └── snowpea.png                ✅ provided (source file was named "ice.png")
├── backgrounds/                  full-screen, 640×480 recommended
│   ├── day.png                    not provided yet — falls back to a flat tint
│   ├── night.png                  "
│   ├── pool.png                   "
│   ├── fog.png                    "
│   └── roof.png                   "
├── zombies/
│   ├── basic/
│   │   └── parts/                head.png, torso.png, arm.png, leg_a.png,
│   │                              leg_b.png, cone.png, bucket.png, flag.png,
│   │                              helmet.png — shared body rig, ✅ provided.
│   │                              Conehead/Buckethead/Flag/Football reuse
│   │                              this same rig + one of the accessories.
│   └── <28 other type slugs>/
│       └── full.png              ✅ provided for all of them (a single
│                                  background-removed cutout each; see
│                                  zombie.h for the full slug list and the
│                                  "rigged vs simple-cutout" split)
└── ui/
    ├── sun.png                  32×32 px — the falling/collectible sun
    └── cursor.png               32×32 px — optional custom IR reticle
                                  (omit to keep the built-in crosshair)
```

Two files are allowed to share a filename across folders (e.g.
`seeds/peashooter.png` and `plants/peashooter.png`) since each is loaded by
its full path, not a compiled-in symbol name — no collision.

## Exact on-screen box each image is drawn into

| Asset | Drawn at | Notes |
|---|---|---|
| `seeds/*.png` | 48×48, centered in a 52×56 seed-bar slot | Engine draws the slot border/background itself — just supply the icon |
| `plants/*.png` | lawn cell minus 6px padding on each side (~50×64 at the default 62×76 cell) | Also used, tinted semi-transparent, for the "held" preview while carrying a seed |
| `ui/sun.png` | 32×32, centered on the falling sun's position | |
| `ui/cursor.png` | 32×32, centered on the Wiimote IR pointer | |
| `font.ttf` | any TrueType/OpenType font | Used for HUD text and the options menu; omit to hide text (gameplay doesn't depend on it) |

## Not covered by this pass

- **Homebrew Channel icon** — `icon.png`, **128×48**, at the project root
  (already documented in the main README).
- **Disc banner (`opening.bnr`)** — currently auto-generated as a blank/
  text-only banner by `scripts/generate_opening_bnr.py`. A fully custom
  image banner uses a different (TPL-based) format; if you want one later,
  the common approach is building it with CustomizeMii rather than a plain
  PNG drop-in.
- **Audio** (SFX/music) — no audio files or code hookup yet. `Settings.volume`
  is already loaded/saved by the options menu, ready for an `audio.cpp` to
  read from once ASND is wired up.
