# tools/devkitpro/

## Recommended: run the setup script

```bat
scripts\setup_toolchain.bat
```

This downloads a portable devkitPPC toolchain and builds GRRLIB straight
into this folder -- nothing installed system-wide, no environment variables,
no PATH changes. Safe to re-run; it skips anything already done. Read
`scripts/setup_toolchain.ps1` if you want to see exactly what it does.

Everything below is the manual alternative, e.g. if you already have
devkitPro installed elsewhere and just want to point at it.

---

Put devkitPro here — at the same level as `tools/wit/` — so the whole
toolchain lives inside the project instead of a system-wide location.

The Makefile looks for `tools/devkitpro/devkitPPC/wii_rules` before falling
back to anything else, so once it's here, no environment variables and no
PATH changes are needed at all.

## What needs to end up in this folder

devkitPro isn't a single binary like WIT — it's a whole directory tree
(`devkitPPC/`, `libogc/`, `portlibs/`, etc.). This folder should end up
containing that tree directly, e.g.:

```
tools/devkitpro/
├── devkitPPC/
│   ├── bin/
│   ├── wii_rules
│   └── ...
├── libogc/
└── portlibs/
```

You still install devkitPro the normal way first (its installer handles
GPG keys, msys2/pacman, etc. — there's no way around that part). The
difference is what you do *after*: instead of leaving it wherever it
installed and pointing environment variables at it, you bring that install
into the project.

## Option A — symlink/junction (recommended)

Keeps the install in its default location (so other projects can still use
it) but makes it appear inside this project too. No file duplication.

**Linux/macOS:**
```bash
ln -s /opt/devkitpro tools/devkitpro
```

**Windows (as Administrator, in cmd):**
```bat
mklink /J tools\devkitpro C:\devkitPro
```

## Option B — copy the whole folder in

Works the same, just duplicates the install (devkitPro is several hundred
MB+, so this is slower and uses more disk):

**Linux/macOS:**
```bash
cp -a /opt/devkitpro tools/devkitpro
```

**Windows:**
```bat
xcopy /e /i /y C:\devkitPro tools\devkitpro
```

## Using a different location instead

If you'd rather keep devkitPro exactly where its installer put it and not
relocate/symlink anything, that still works — just set `DEVKITPRO` (or
`DEVKITPPC`) as you would have before:

```bash
export DEVKITPRO=/opt/devkitpro
```

The Makefile checks for an explicitly-set `DEVKITPRO`/`DEVKITPPC` first and
`tools/devkitpro/` second -- so either approach keeps working. (It no longer
guesses at system install locations; if neither is found it stops with a
clear error instead of silently picking up an unrelated install.)

Contents of this folder are ignored by git (see `.gitignore`), so a local
symlink or copy here never gets committed.
