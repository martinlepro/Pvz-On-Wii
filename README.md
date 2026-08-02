# Plants vs. Zombies Wii (Homebrew)

Open-source **Plants vs. Zombies**–inspired lawn defense game for the **Nintendo Wii**, built with **devkitPPC**, **libogc**, and **GRRLIB**.

> Fan project — not affiliated with PopCap / EA. Use your own or freely licensed assets for sprites and audio.

## Features (v0.2)

- 9×5 planting grid with sun economy, plus falling suns you collect by pointing at them
- Seed bar with six plant types (placeholder colored tiles / icons)
- **Wiimote**
  - **A / B (hold):** grab the selected seed from the top bar, shown semi-transparent
  - **A / B (release):** plant at the tile-cursor, or cancel if it's been pushed off the lawn
  - **1:** move the tile-cursor one tile right
  - **2:** move the tile-cursor one tile left
  - **- / +:** open the options menu (adjust volume, remap controls); press again to close
  - **IR pointer:** hover a seed-bar slot to select it, or hover a falling sun to collect it
  - **HOME:** exit (never remappable)
- **Nunchuk (optional):** analog stick moves the tile-cursor one tile per flick, in any direction
- All controls above (except HOME) are remappable from the options menu, and the choice — along with volume — is saved to `sd:/apps/pvz_wii/settings.cfg`
- See [ASSETS.md](ASSETS.md) for the exact image paths/filenames/dimensions this build expects once art is added — nothing needs to change in code, textures are picked up automatically

## Project layout

```
plant_vs_zombie_wii/
├── Makefile
├── meta.xml                  # Homebrew Channel metadata
├── LICENSE                   # MIT
├── include/                  # Headers
├── source/                   # Game source (.cpp)
├── assets/                   # Runtime game assets (also copied to disc)
├── disc/
│   ├── assets/               # Static disc files (opening.bnr, files/)
│   ├── meta/
│   │   ├── disc.info         # Title + 4-char ID for WIT
│   │   └── COMPONENTS.md     # Ticket/TMD/boot explained
│   └── staging/              # Assembled tree (gitignored, built by scripts)
├── output/
│   ├── homebrew/             # boot.dol + meta.xml for HBC
│   └── disc/                 # pvz_wii.iso + pvz_wii.wbfs
├── tools/                    # Project-local toolchain, nothing on system PATH
│   ├── devkitpro/            # devkitPro install (or symlink), see its README
│   └── wit/                  # WIT binary, see its README
└── scripts/
    ├── setup_toolchain.bat       # One-time: fetches devkitPro+GRRLIB+WIT into tools/
    ├── setup_toolchain.ps1       # (called by the .bat above)
    ├── build_iso.sh          # ISO packer (used by `make iso`)
    ├── build_iso.bat         # Same, for Windows CMD without MSYS
    └── generate_opening_bnr.py
```

## Build via GitHub Actions (recommended)

Push this repo to GitHub (or fork it) and the `.github/workflows/build.yml`
workflow builds `pvz_wii.iso` / `.wbfs` automatically -- entirely on GitHub's
servers. Nothing runs on your machine, nothing downloads to your machine, and
there's no local toolchain, environment variable, or PATH to set up.

1. Push to GitHub (or click **Run workflow** on the **Actions** tab to run it
   on demand).
2. Wait for the green check on the run.
3. Open the run and download **pvz_wii-build** from its **Artifacts** section
   at the bottom of the page -- it contains the `.iso`, `.wbfs`, and the
   homebrew `boot.dol` bundle.

This is the easiest path if you just want the ISO and don't want to fight a
local Windows toolchain setup. The local setup below is only needed if you
want to iterate/build on your own machine without waiting on CI each time.

## Requirements (local build, optional)

**Windows: run `scripts\setup_toolchain.bat`.** It downloads a portable
devkitPPC toolchain, builds GRRLIB, and fetches WIT, all into `tools/`
next to this README. No installer, no environment variables, no PATH
changes, and nothing outside this project folder. Safe to re-run.

Once that's done, skip to [Build workflow](#build-workflow) below.

<details>
<summary>Manual setup instead (any OS, or if you already have devkitPro installed elsewhere)</summary>

1. [devkitPro](https://devkitpro.org/) with `devkitPPC`
2. **libogc** (included with devkitPPC)
3. **GRRLIB** — install into `$(DEVKITPRO)/libogc` per [GRRLIB docs](https://github.com/GRRLIB/GRRLIB) (a one-time library install, not a PATH change)
4. **Wiimms ISO Tools (WIT)** — only for `make iso` / disc images
   Download: [wit.wiimm.de](https://wit.wiimm.de/)

**No system PATH changes are needed for any of this**, and devkitPro itself
lives inside the project, right next to WIT:

- Install devkitPro normally once (its installer handles GPG keys/msys2/pacman — unavoidable), then put the resulting install — or just a symlink to it — at `tools/devkitpro/`, at the same level as `tools/wit/`. See [tools/devkitpro/README.md](tools/devkitpro/README.md) for the exact one-line command per OS.
- With devkitPro at `tools/devkitpro/`, the Makefile finds it automatically — no `DEVKITPRO`/`DEVKITPPC` environment variables required, and nothing added to PATH.
- WIT works the same way: drop the `wit`/`wit.exe` binary into `tools/wit/` (see [tools/wit/README.md](tools/wit/README.md)), and `make iso` finds it automatically.
- Prefer to keep devkitPro in its default system install location instead of relocating/symlinking it? That still works — just `export DEVKITPRO=/opt/devkitpro` (or `DEVKITPPC`) for that shell; the Makefile checks an explicit env var first and `tools/devkitpro/` second (it no longer guesses at system paths).

On Windows with MSYS2/devkitPro, use the devkitPro shell.

</details>

See [BUILD_ISO_TUTORIAL.md](BUILD_ISO_TUTORIAL.md) for a full step-by-step walkthrough of building the ISO with no PATH configuration.

## Build workflow


### Homebrew only (default)

```bash
make
```

| Output | Path |
|--------|------|
| DOL (dev) | `pvz_wii.dol` (project root) |
| HBC bundle | `output/homebrew/boot.dol` + `meta.xml` |

Copy `output/homebrew/` to `/apps/pvz_wii/` on your SD card.

### Standalone Wii disc (ISO + WBFS)

```bash
make opening-bnr   # optional: generate disc banner
make iso
```

| Output | Path | Use |
|--------|------|-----|
| ISO | `output/disc/pvz_wii.iso` | Dolphin, disc loaders |
| WBFS | `output/disc/pvz_wii.wbfs` | Compressed backup format |

WIT reads `disc/staging/` (assembled automatically), generates ticket/TMD/boot partition internals, and writes the final image. See `disc/meta/COMPONENTS.md` for details.

**Dolphin:** File → Open → `output/disc/pvz_wii.iso`

### Windows CMD (without `make iso`)

After `make` produces `pvz_wii.dol`:

```bat
scripts\build_iso.bat
```

### Other make targets

| Target | Description |
|--------|-------------|
| `make homebrew` | Refresh `output/homebrew/` from existing `.dol` |
| `make wbfs` | Alias for `make iso` (both formats are built) |
| `make opening-bnr` | Write `disc/assets/opening.bnr` from `disc.info` |
| `make clean` | Remove build artifacts and packaged outputs |
| `make help` | List targets |

## Run on Wii

**Homebrew Channel** — copy to SD card:

```
/apps/pvz_wii/boot.dol
/apps/pvz_wii/meta.xml
```

**Disc loaders / modded disc channel** — use `output/disc/pvz_wii.iso` or `.wbfs`.

> **Disc-native asset loading is experimental.** `source/discfst.cpp`
> tries to read `assets/` directly off the disc via `DI_OpenPartition()`/
> `DI_Read()` — a libogc API that asks IOS itself to decrypt the
> partition (no key material is embedded in this project). libogc's own
> docs note this "will only work on original discs", so whether it
> works against this homebrew-built disc depends on the IOS/loader
> environment — it may or may not succeed. It fails safely either way.
>
> **If you launch a disc/USB-loader build with no assets on SD/USB**, an
> on-screen debug line (bottom-left, next to the missing-texture count)
> reports what happened: `DI not ready / no disc`, `no DATA partition
> found`, `DI_OpenPartition failed (code N)`, `opened but FST looked
> wrong`, or `mounted OK`. If it's anything other than "mounted OK",
> fall back to SD or USB with `apps/pvz_wii/assets/...` (same layout as
> the Homebrew Channel) — see the header comment in
> `source/discfst.cpp` for the full technical story.

Optional: add `icon.png` (128×48) at project root and rebuild for a custom HBC banner.

## Controls summary

| Input | Action |
|-------|--------|
| A / B hold | Pick up selected seed (shown semi-transparent) |
| A / B release | Plant at the tile-cursor, or cancel if pushed off the lawn |
| 1 | Move tile-cursor one tile right |
| 2 | Move tile-cursor one tile left |
| Nunchuk stick | Move tile-cursor one tile per flick (any direction) |
| IR pointer | Hover a seed to select it, or a falling sun to collect it |
| - / + | Open options menu (volume, remap controls); press again to close |
| HOME | Exit (not remappable) |

Inside the options menu: D-Pad Up/Down to navigate, Left/Right to adjust the volume slider, A to select/rebind, B to go back. Volume and control bindings are saved to `sd:/apps/pvz_wii/settings.cfg` whenever the menu is closed.

## Extending the clone

Suggested next modules (not yet implemented):

- `zombie.cpp` — wave spawner, walking, eating plants
- `projectile.cpp` — peas, collision with zombies
- `audio.cpp` — ASND / libogc sound effects, reading `Settings::volume` (already loaded/saved; just needs `ASND_Init` + a mixer wired to it)
- Drop PNGs into `assets/` per [ASSETS.md](ASSETS.md) — `render.cpp` already tries to load each one at startup and only falls back to a colored rectangle if the file isn't there, so no code changes are needed to start seeing real art

## License

MIT — see [LICENSE](LICENSE).
