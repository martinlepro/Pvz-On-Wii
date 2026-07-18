#!/usr/bin/env python3
"""
generate_opening_bnr.py — Create a minimal Wii opening.bnr (BNR1 + IMET) for homebrew discs.

The banner is optional but improves how the title appears in Dolphin and USB loaders.
Output: disc/assets/opening.bnr (reads title from disc/meta/disc.info when present)

Reference: WiiBrew BNR / IMET format
https://wiibrew.org/wiki/Opening.bnr
"""
from __future__ import annotations

import struct
import sys
from pathlib import Path

ROOT = Path(__file__).resolve().parent.parent
DISC_INFO = ROOT / "disc" / "meta" / "disc.info"
OUT_PATH = ROOT / "disc" / "assets" / "opening.bnr"

DEFAULT_TITLE = "Plants vs Zombies Wii"
DEFAULT_CODE = "PVZW"


def read_disc_info() -> tuple[str, str]:
    title = DEFAULT_TITLE
    code = DEFAULT_CODE
    if DISC_INFO.is_file():
        for line in DISC_INFO.read_text(encoding="utf-8").splitlines():
            line = line.strip()
            if line.startswith("#") or "=" not in line:
                continue
            key, val = line.split("=", 1)
            key, val = key.strip(), val.strip()
            if key == "DISC_TITLE":
                title = val
            elif key == "DISC_ID":
                code = val[:4].ljust(4)
    return title, code


def utf16_be(s: str, length: int) -> bytes:
    raw = s.encode("utf-16-be")
    if len(raw) > length:
        raw = raw[: length - 2]
    return raw.ljust(length, b"\x00")


def build_imet(title: str, code: str) -> bytes:
    """Build the IMET block (standard Wii banner metadata).

    Layout: 0x10-byte header, then 7 title slots of 0x40 bytes each
    (0x10..0x1D0), followed by the game/maker code and rating fields.
    The buffer must be sized to fit all of that (0x1E0 bytes) — it is
    NOT 0x180: that would end 0x50 bytes before the last title slot
    finishes, so the trailing fields would silently overlap and corrupt
    the 7th (Dutch) title instead of landing in their own space.
    """
    TITLE_COUNT = 7
    TITLE_SIZE = 0x40
    TITLES_START = 0x10
    TITLES_END = TITLES_START + TITLE_COUNT * TITLE_SIZE  # 0x1D0
    TOTAL_SIZE = TITLES_END + 0x10  # room for code/maker/rating + padding = 0x1E0

    buf = bytearray(TOTAL_SIZE)
    struct.pack_into(">4s", buf, 0x00, b"IMET")
    struct.pack_into(">I", buf, 0x04, 0x00000200)  # version
    struct.pack_into(">I", buf, 0x08, 0x00000003)  # request count
    struct.pack_into(">I", buf, 0x0C, 0x00000001)  # flags

    # Titles (Japanese, English, German, French, Spanish, Italian, Dutch)
    for i, t in enumerate([title] * TITLE_COUNT):
        off = TITLES_START + i * TITLE_SIZE
        buf[off : off + TITLE_SIZE] = utf16_be(t, TITLE_SIZE)

    # Game code (4 ASCII) + maker code (2), placed right after the title
    # table so it can no longer overlap the last title slot.
    struct.pack_into(">4s", buf, TITLES_END, code.encode("ascii", "replace")[:4])
    struct.pack_into(">2s", buf, TITLES_END + 0x04, b"01")  # maker/developer code placeholder
    struct.pack_into(">B", buf, TITLES_END + 0x06, 0x00)    # age / rating
    return bytes(buf)


def build_bnr1(title: str, code: str) -> bytes:
    """BNR1 header (0x60 bytes) + single IMET (0x180 bytes)."""
    header = bytearray(0x60)
    struct.pack_into(">4s", header, 0x00, b"BNR1")
    struct.pack_into(">I", header, 0x04, 0x00000001)  # version
    imet = build_imet(title, code)
    return bytes(header) + imet


def main() -> int:
    title, code = read_disc_info()
    OUT_PATH.parent.mkdir(parents=True, exist_ok=True)
    data = build_bnr1(title, code)
    OUT_PATH.write_bytes(data)
    print(f"Wrote {OUT_PATH} ({len(data)} bytes)")
    print(f"  Title: {title}")
    print(f"  ID:    {code}")
    return 0


if __name__ == "__main__":
    sys.exit(main())
