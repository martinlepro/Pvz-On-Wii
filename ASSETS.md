# Asset manifest

## 2026-07 fix: plants that were mis-filed under `assets/zombies/`

11 plants had their art dropped under `assets/zombies/<name>/` (some with a
typo'd slug) instead of `assets/plants/`, and most only had the rigged-zombie
`parts/` breakdown (head/torso/arm/leg_a/leg_b) with no single `full.png`
cutout. On top of that, `s_seedIconPaths`/`s_plantSpritePaths` in
`render.cpp` only had explicit entries for the first 8 `PlantType` values —
every plant after `PLANT_REPEATER` silently fell back to `NULL` (C zero-fills
missing initializers), so none of them ever got a seed icon or lawn sprite.

Fixed:
- `chomper`, `potato_mine`, `puff_shroom`, `sun_shroom`, `ice_shroom`,
  `gatling_pea`, and `grave_buster`/`gravebuster` moved out of
  `assets/zombies/` into `assets/plants/` + `assets/seeds/`.
- `hipno_shroom` → renamed `hypno_shroom`, `scraredy_shroom` → renamed
  `scaredy_shroom`, `fumeshroom` → renamed `fume_shroom`, `zombie_japaleno` →
  renamed `jalapeno` (typos in the original folder names).
- The 9 that only had rig `parts/` were recomposed into a single cutout by
  replaying the exact same pose math `DrawRiggedZombie()` already uses
  (idle stance, no source art was invented — same pixels, reassembled).
  Worth a visual sanity check once art can be viewed in-game; swap in a
  proper single-pose export later if you have the original source sheet.
- `s_seedIconPaths` / `s_plantSpritePaths` now list all `PLANT_TYPE_COUNT`
  (49) entries explicitly (real path or explicit `NULL`), so this exact bug
  class (silently-truncated initializer list) can't happen again.
- Removed two stray duplicate folders that weren't used by any code path:
  `assets/zombies/zombie__wall_nut` (typo dupe of `zombie_wall_nut`) and
  `assets/zombies/pole vaulting` (space-typo dupe of `pole_vaulting`).

Known remaining gap (not fixed, no source art available):
- `assets/zombies/bobsleg_team/` and `assets/zombies/catapult_2/` have full
  art sitting unused — there's no matching `ZombieType` entry for either yet,
  so nothing loads them. Left in place in case you want to wire up a Bobsled
  Team zombie / alternate catapult pose later.

## 2026-07 update: every plant + zombie now has art, and the "7 textures not found" debug counter is back to 0

Two independent things were making `Render_GetMissingTextureCount()` read 7:
- **6 of them**: `ZOMBIE_LADDER`'s `assetSlug` is `"ladder"`, and
  `assets/zombies/ladder/` didn't exist at all, so all 6 of its texture loads
  (5 rig parts + `full.png`) failed every frame.
- **1 of them**: `Menu_Init()` unconditionally tries
  `assets/background/menu.png` before falling back to the lawn background —
  that file never existed, so it counted as "not found" once per boot even
  though the fallback made it invisible on screen.

Fixed:
- **All 49 `PlantType`s now have a seed icon + lawn sprite** (previously 17).
  The 32 new ones were background-removed and cropped from the reference
  sheets in the `PvZ-Wii-color-palette` repo — each source PNG there is
  actually a large multi-variant contact sheet (dozens of palette-swapped
  copies of the same character on one canvas, not a single image), so
  extraction picks out the single largest clean silhouette on each sheet.
  **5 plants had no reference art in either archive at all** — Doom-shroom,
  Plantern, Cactus, Gloom-shroom, and Cob Cannon are simple *original*
  placeholder icons drawn from scratch (flat shapes, not a redraw of any
  copyrighted reference) so every plant shows *something* recognizable
  instead of a flat box; swap in real art for these five whenever you have
  it — `s_seedIconPaths`/`s_plantSpritePaths` just need the path updated.
- **`ZOMBIE_LADDER` now has art**: since no source existed, it reuses Basic
  Zombie's existing rig (head/torso/arm/leg_a/leg_b + full.png — correct
  anyway, since Ladder Zombie in the original game *is* a basic zombie
  carrying a prop) plus a small original ladder graphic composited onto the
  torso/full images so it doesn't look identical to a plain Basic Zombie.
- **`assets/background/menu.png` added** — an original title-screen
  composition (sky/sun/clouds over a striped lawn, same family as the
  in-level day background but distinct), so the title screen no longer
  silently depends on the lawn-background fallback.
- **Two gameplay bugs found while chasing this down, also fixed:**
  - `ZOMBIE_LADDER` had full stats and its "leaves a ladder over the first
    plant" behavior in `zombie.cpp`, but was never added to `level.cpp`'s
    `kUnlockSchedule` — it could never actually spawn in a game. Added at
    unlock level 13.
  - `Plant_GetRewardForLevel()` special-cased "bonus/conveyor" levels
    (`levelIndex % 10 == 4 or 9`) to never grant a reward. But all 49 plants
    are scheduled one-per-level across levels 0–48 with no gaps, so that
    special case was silently swallowing the reward popup for the 9 plants
    that happened to land on one of those levels (Potato Mine, Sun-shroom,
    Ice-shroom, Tangle Kelp, Sea-shroom, Starfruit, Kernel-pult, Melon-pult,
    Winter Melon) — they were still selectable, just with no "you got a new
    plant!" moment. Removed the special case; a scheduled plant is always
    granted.
- Removed 13 more stray/duplicate asset folders confirmed unused by any
  code path (on top of the 2 already removed): the leftover
  `assets/zombies/{chomper,fumeshroom,gravebuster,hipno_shroom,ice_shroom,
  potato_mine,puff_shroom,scraredy_shroom,sun_shroom,gatling_pea,
  zombie_japaleno}` copies from before those plants were moved to
  `assets/plants/`, plus `zombie__wall_nut` and `pole vaulting` (the doc
  above already claimed these last two were gone, but they were still on
  disk in this archive — actually deleted now).

`Render_GetMissingTextureCount()` is 0 against the current `assets/` tree.

## 2026-07 follow-up: pea-shaped projectiles now use a real sprite

Every shot in this game (`DrawProjectiles` in `render.cpp`) was a flat GX
circle — including Peashooter's own pea, since no projectile art existed
anywhere in either archive. `assets/projectiles/pea.png` is now extracted
from the same reference sheet as `assets/plants/peashooter.png` (the small
"pea" callout on that sheet) and is drawn, tinted per-kind via the existing
`ProjectileColor()` table, for the 4 projectile kinds that are visually
"a pea ball": `PROJ_PEA` (Peashooter/Repeater/Gatling Pea/Threepeater/Split
Pea), `PROJ_SNOW`, `PROJ_FIRE`, and `PROJ_FUME`. The other 9 kinds (cabbage,
corn kernel, melon, spore, star, spike, homing...) still draw as a plain
circle — those are visually distinct shapes, not a tinted pea, and had no
matching reference art to extract either. Say the word if you'd like those
covered too; the fallback-to-circle pattern already there extends the same way.

Every sprite below is loaded **at runtime** from a plain relative path (the
same way `assets/font.ttf` already worked), and `render.cpp` gracefully
falls back to a flat colored rectangle for anything that isn't present.
That means:

- You can drop in a replacement file at any time; nothing else needs to
  change, since paths are matched by filename, not compiled in.
- Wrong dimensions won't break anything — everything is scaled to fit its
  on-screen box automatically. The sizes below are *recommended* (for
  crispness / texture-memory reasons on real Wii hardware), not mandatory.
- All files are plain PNG with transparency (RGBA).

Run `make` (or `make homebrew` / `make iso`) again after adding/replacing
files — the build already copies the whole `assets/` folder into both the
Homebrew Channel bundle and the disc image for you (no Makefile changes
needed).

## Where things go

```
assets/
├── font.ttf              (HUD/menu text)
├── seeds/                 48×48 px — icon shown in the seed-bar slot
│   └── <49 files>.png    ✅ all PLANT_TYPE_COUNT plants provided
├── plants/                recommended 64×80 px — sprite on the lawn
│   └── <49 files>.png    ✅ all PLANT_TYPE_COUNT plants provided (44 real
│                          cutouts + 5 original placeholder icons — see the
│                          2026-07 update above for which five)
├── background/             full-screen, 640×480 recommended
│   ├── every lawn day.png ✅ Day/world background
│   ├── night.png          ✅ Night
│   ├── pool.png           ✅ Pool
│   ├── fog.png            ✅ Fog
│   ├── roof.png           ✅ Roof
│   └── menu.png           ✅ title screen (added in the 2026-07 update)
├── zombies/
│   └── <32 type slugs>/    one folder per ZombieType, ✅ all provided
│       ├── full.png            single-pose cutout (fallback if parts/ missing)
│       └── parts/               head.png, torso.png, arm.png, leg_a.png,
│                                 leg_b.png — every type gets full procedural
│                                 limb animation from these, not just the
│                                 humanoid-with-accessory subset
├── projectiles/
│   └── pea.png              ✅ shared sprite for the 4 pea-shaped projectile
│                             kinds (PROJ_PEA/SNOW/FIRE/FUME), tinted per-kind
└── ui/
    ├── sun.png              32×32 px — the falling/collectible sun, ✅ provided
    └── cursor.png           32×32 px — optional custom IR reticle, ✅ provided
```

`assets/button/*.png` (the 4 title-screen mode buttons) are also all
present and aren't listed above since `menu.cpp`, not `render.cpp`, loads
those — see `Menu_Init()`.

Two files are allowed to share a filename across folders (e.g.
`seeds/peashooter.png` and `plants/peashooter.png`) since each is loaded by
its full path, not a compiled-in symbol name — no collision.

## Exact on-screen box each image is drawn into

| Asset | Drawn at | Notes |
|---|---|---|
| `seeds/*.png` | 48×48, centered in a 52×56 seed-bar slot | Engine draws the slot border/background itself — just supply the icon |
| `plants/*.png` | lawn cell minus 6px padding on each side (~50×64 at the default 62×76 cell) | Also used, tinted semi-transparent, for the "held" preview while carrying a seed |
| `projectiles/pea.png` | square box sized `2×ProjectileRadius(kind)`, centered on the shot | Tinted per-kind via `ProjectileColor()` — see DrawProjectiles |
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
