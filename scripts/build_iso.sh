#!/usr/bin/env bash
# build_iso.sh — Stage disc tree and pack pvz_wii.iso / pvz_wii.wbfs with WIT
# Usage: ./scripts/build_iso.sh [path/to/game.dol]
# WIT does not need to be on PATH — see the WIT_PATH section below.
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
cd "$ROOT"

# ---------------------------------------------------------------------------
# WIT (Wiimms ISO Tools) location
#
# WIT does NOT need to be on your system PATH. WIT_PATH is normally set for
# you by `make iso`. If you're running this script directly instead, either:
#
#   - copy the wit binary into tools/wit/ inside the project root:
#       Linux/macOS -> tools/wit/wit   (chmod +x it)
#       Windows/MSYS -> tools/wit/wit.exe
#     it will be picked up automatically below, or
#   - point WIT_PATH at your existing install:
#       WIT_PATH=/path/to/wit ./scripts/build_iso.sh
#
# Download WIT: https://wit.wiimm.de/
# ---------------------------------------------------------------------------
WIT_PATH="${WIT_PATH:-$ROOT/tools/wit/wit}"
if [[ ! -x "$WIT_PATH" && -x "$WIT_PATH.exe" ]]; then
  WIT_PATH="$WIT_PATH.exe"
fi

TARGET="${TARGET:-pvz_wii}"
DOL="${1:-$ROOT/$TARGET.dol}"
META="$ROOT/disc/meta/disc.info"
STAGING="$ROOT/disc/staging"
HB_OUT="$ROOT/output/homebrew"
ISO_OUT="$ROOT/output/disc"
ASSETS="$ROOT/disc/assets"

# ---------------------------------------------------------------------------
# Load disc.info (KEY=VALUE)
# ---------------------------------------------------------------------------
if [[ ! -f "$META" ]]; then
  echo "error: missing $META" >&2
  exit 1
fi

# Parsed manually (not sourced/eval'd) so values containing spaces, such as
# DISC_TITLE, can never be misinterpreted as shell syntax.
while IFS= read -r line; do
  line="${line%$'\r'}"
  [[ "$line" =~ ^([A-Z_]+)=(.*)$ ]] || continue
  key="${BASH_REMATCH[1]}"
  val="${BASH_REMATCH[2]}"
  case "$key" in
    DISC_TITLE|DISC_ID|DISC_REGION|DISC_IOS) printf -v "$key" '%s' "$val" ;;
  esac
done < "$META"

DISC_TITLE="${DISC_TITLE:-Plants vs Zombies Wii}"
DISC_ID="${DISC_ID:-PVZW}"
DISC_REGION="${DISC_REGION:-E}"

# ---------------------------------------------------------------------------
# Prerequisites
# ---------------------------------------------------------------------------
if [[ ! -f "$DOL" ]]; then
  echo "error: DOL not found: $DOL (run 'make' first)" >&2
  exit 1
fi

if [[ ! -x "$WIT_PATH" ]]; then
  echo "error: 'wit' executable not found (or not executable) at: $WIT_PATH" >&2
  echo "       Copy Wiimms ISO Tools into tools/wit/ inside the project," >&2
  echo "       or set WIT_PATH to point at your install, e.g.:" >&2
  echo "         WIT_PATH=/path/to/wit ./scripts/build_iso.sh" >&2
  echo "       Download WIT: https://wit.wiimm.de/" >&2
  exit 1
fi

echo "==> Using wit: $WIT_PATH"
echo "==> WIT version: $("$WIT_PATH" VERSION 2>/dev/null | head -n1 || "$WIT_PATH" --version 2>/dev/null || echo unknown)"

# ---------------------------------------------------------------------------
# Stage disc filesystem (mirrors retail layout: sys/main.dol + files/)
# ---------------------------------------------------------------------------
echo "==> Staging disc tree at $STAGING"
rm -rf "$STAGING"
mkdir -p "$STAGING/sys" "$STAGING/files" "$HB_OUT" "$ISO_OUT"

cp "$DOL" "$STAGING/sys/main.dol"

if [[ -d "$ASSETS/files" ]]; then
  shopt -s nullglob dotglob
  for item in "$ASSETS/files"/*; do
    cp -a "$item" "$STAGING/files/"
  done
  shopt -u nullglob dotglob
fi

if [[ -d "$ROOT/assets" ]]; then
  mkdir -p "$STAGING/files/assets"
  shopt -s nullglob
  for item in "$ROOT/assets"/*; do
    [[ -e "$item" ]] || continue
    cp -a "$item" "$STAGING/files/assets/"
  done
  shopt -u nullglob
fi

if [[ -f "$ASSETS/opening.bnr" ]]; then
  cp "$ASSETS/opening.bnr" "$STAGING/opening.bnr"
  echo "    included opening.bnr"
else
  echo "    note: disc/assets/opening.bnr not found (optional banner skipped)"
fi

# Homebrew Channel bundle (separate from disc output)
cp "$DOL" "$HB_OUT/boot.dol"
cp "$ROOT/meta.xml" "$HB_OUT/meta.xml"
if [[ -f "$ROOT/icon.png" ]]; then
  cp "$ROOT/icon.png" "$HB_OUT/icon.png"
fi
if [[ -d "$ROOT/assets" ]]; then
  mkdir -p "$HB_OUT/assets"
  shopt -s nullglob
  for item in "$ROOT/assets"/*; do
    [[ -e "$item" ]] || continue
    cp -a "$item" "$HB_OUT/assets/"
  done
  shopt -u nullglob
fi

# ---------------------------------------------------------------------------
# Pack with WIT — generates ticket, TMD, boot partition, and FST internally
# ---------------------------------------------------------------------------
ISO_FILE="$ISO_OUT/$TARGET.iso"
WBFS_FILE="$ISO_OUT/$TARGET.wbfs"

WIT_FLAGS=(--overwrite --name "$DISC_TITLE" --id "$DISC_ID")
if [[ -n "$DISC_REGION" ]]; then
  WIT_FLAGS+=(--region "$DISC_REGION")
fi

echo "==> Building WBFS: $WBFS_FILE"
"$WIT_PATH" copy "$STAGING/" "$WBFS_FILE" "${WIT_FLAGS[@]}"

echo "==> Building ISO:  $ISO_FILE"
"$WIT_PATH" copy "$STAGING/" "$ISO_FILE" --iso "${WIT_FLAGS[@]}"

echo ""
echo "Done."
echo "  Homebrew:  $HB_OUT/boot.dol"
echo "  ISO:       $ISO_FILE"
echo "  WBFS:      $WBFS_FILE"
echo ""
echo "Open in Dolphin: File -> Open -> $ISO_FILE"
