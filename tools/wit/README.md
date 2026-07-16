# tools/wit/

## Recommended: run the setup script

```bat
scripts\setup_toolchain.bat
```

Downloads WIT straight into this folder alongside devkitPro. Safe to re-run.

Everything below is the manual alternative.

---

Put your local copy of **Wiimms ISO Tools (WIT)** here — it is *not* required
to be on your system PATH.

Download WIT: https://wit.wiimm.de/

## Where to put the binary

| Platform     | Path                     |
|--------------|--------------------------|
| Linux/macOS  | `tools/wit/wit`          |
| Windows      | `tools/wit/wit.exe`      |

After copying the binary in, on Linux/macOS make it executable:

```sh
chmod +x tools/wit/wit
```

With the binary in place, `make iso` (or `scripts/build_iso.sh` /
`scripts/build_iso.bat` directly) will find it automatically — no PATH
changes needed.

## Using a different location instead

If you'd rather keep your WIT install somewhere else, don't move files
around — just point `WIT_PATH` at it:

```sh
make iso WIT_PATH=/path/to/wit
# or
export WIT_PATH=/path/to/wit
```

On Windows (cmd):

```bat
set "WIT_PATH=C:\path\to\wit.exe"
scripts\build_iso.bat
```

Binaries placed in this folder are ignored by git (see `.gitignore`) so you
don't need to worry about accidentally committing them.
