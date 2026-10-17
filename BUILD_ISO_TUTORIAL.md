# Building the ISO — step by step (no PATH editing required)

This walks through going from a fresh clone of this project to a working
`pvz_wii.iso` you can open in Dolphin, with zero system PATH changes at any
point. Everything WIT-related is already project-local; the `Makefile`
changes in this version make devkitPPC project-friendly too.

## 1. Install devkitPro, then bring it into the project (one time, per machine)

Installing devkitPro itself is the only unavoidable "install" step — it's the
actual PowerPC compiler/linker used to build Wii executables. Get it from
[devkitpro.org](https://devkitpro.org/wiki/Getting_Started):

- **Windows:** run the official devkitPro installer, check `devkitPPC` in the
  component list.
- **Linux/macOS:** follow devkitPro's `dkp-pacman` instructions, then
  `sudo dkp-pacman -S devkitPPC wii-dev`.

That installer step still puts devkitPro in its normal system location
(`/opt/devkitpro` or `C:\devkitPro`) — there's no way around that part. What
you do next is bring it into the project, at the same level as `tools/wit/`:

```bash
# Linux/macOS — symlink, no duplication:
ln -s /opt/devkitpro tools/devkitpro
```

```bat
:: Windows (as Administrator):
mklink /J tools\devkitpro C:\devkitPro
```

See [tools/devkitpro/README.md](tools/devkitpro/README.md) for the copy-instead-of-symlink
option and other details.

Once `tools/devkitpro/devkitPPC/wii_rules` exists, the Makefile finds it
automatically — **no `DEVKITPRO`/`DEVKITPPC` environment variables, and
nothing added to PATH.**

Prefer to leave devkitPro exactly where the installer put it? That still
works too — just `export DEVKITPRO=/opt/devkitpro` for that shell instead of
symlinking. The Makefile checks an explicit env var first, `tools/devkitpro/`
second, and the standard system location last.

## 2. Install GRRLIB (one time, per machine)

Follow the [GRRLIB docs](https://github.com/GRRLIB/GRRLIB) to build/install
it into `$(DEVKITPRO)/libogc`. This is copying library files into your
devkitPro install, same as any other portlib — not a PATH change.

## 3. Get WIT (Wiimms ISO Tools) — project-local, no install

WIT is only needed for `make iso` (packing the disc image). It does **not**
get installed system-wide and does **not** touch PATH:

1. Download WIT from [wit.wiimm.de](https://wit.wiimm.de/).
2. Copy the binary into the project:
   - Linux/macOS → `tools/wit/wit`, then `chmod +x tools/wit/wit`
   - Windows → `tools/wit/wit.exe`

That's it — `make iso` finds it automatically because it lives inside the
project folder. (Prefer a different location? Set `WIT_PATH=/your/path`
when invoking make instead of moving files around.)

## 4. Build the homebrew binary

From the project root:

```bash
make
```

This produces:

| Output | Path |
|--------|------|
| Dev `.dol` | `pvz_wii.dol` |
| HBC bundle | `output/homebrew/boot.dol` + `meta.xml` |

If this step fails with a devkitPPC-not-found error, re-check step 1 — every
other error will name the missing header/library instead.

## 5. (Optional) Generate the disc banner

```bash
make opening-bnr
```

Writes `disc/assets/opening.bnr`, shown as the title's banner in Dolphin and
the Wii System Menu. Skip this and the ISO still boots fine — just with a
blank banner.

## 6. Build the ISO

```bash
make iso
```

Internally this re-runs `make`, stages `disc/staging/` (main.dol + any files
under `assets/` and `disc/assets/files/`), and calls the local
`tools/wit/wit` to pack the images. Output:

| Output | Path | Use |
|--------|------|-----|
| ISO | `output/disc/pvz_wii.iso` | Dolphin, disc loaders |
| WBFS | `output/disc/pvz_wii.wbfs` | Compressed backup format |

On Windows without `make` (plain CMD), build the `.dol` first however you
normally do, then run `scripts\build_iso.bat` directly — it has the same
project-local WIT lookup built in.

## 7. Run it

- **Dolphin:** File → Open → `output/disc/pvz_wii.iso`
- **Real Wii / USB loader:** copy `output/disc/pvz_wii.wbfs` (or the `.iso`)
  to your loader's games folder
- **Homebrew Channel instead of a disc:** copy `output/homebrew/` to
  `/apps/pvz_wii/` on an SD card

## Troubleshooting

| Symptom | Fix |
|---|---|
| `devkitPPC not found` | Symlink/copy devkitPro into `tools/devkitpro/` (step 1), or `export DEVKITPPC=/path/to/devkitPPC` for that terminal |
| `'wit' executable not found` | Put the `wit`/`wit.exe` binary in `tools/wit/` (step 3), or pass `WIT_PATH=/path/to/wit` |
| Missing GRRLIB/freetype/etc. headers | Re-check step 2 — GRRLIB must be installed into `$(DEVKITPRO)/libogc` |
| `make iso` runs but ISO won't boot in Dolphin | Confirm `disc/meta/disc.info` has a valid 4-character `DISC_ID`; re-run `make clean && make iso` |

Nothing above ever requires opening your OS's environment-variable/PATH
settings dialog, or editing `.bashrc`/`.zshrc`/PowerShell profiles.
