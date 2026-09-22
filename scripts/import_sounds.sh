#!/usr/bin/env bash
#
# Fetch the chess.com "default" sound set and install the subset OpenChess uses
# into assets/sounds/. Requires curl and bsdtar (both ship with macOS; on Linux
# install libarchive-tools/bsdtar or unzip).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="$ROOT/assets/sounds"
URL="https://images.chesscomfiles.com/chess-themes/sounds/_MP3_/default.zip"
UA="Mozilla/5.0 (Macintosh; Intel Mac OS X 10_15_7) AppleWebKit/605.1.15"

SOUNDS=(
    move-self move-opponent capture castle move-check promote
    illegal notify game-end game-win game-lose game-draw
)

mkdir -p "$DEST"
tmp="$(mktemp -d)"
trap 'rm -rf "$tmp"' EXIT

echo "Downloading $URL ..."
curl -fsSL -A "$UA" "$URL" -o "$tmp/default.zip"

if command -v bsdtar >/dev/null 2>&1; then
    bsdtar -xf "$tmp/default.zip" -C "$tmp" standard
elif command -v unzip >/dev/null 2>&1; then
    unzip -oq "$tmp/default.zip" -d "$tmp"
else
    echo "error: need bsdtar or unzip to extract the archive" >&2
    exit 1
fi

for s in "${SOUNDS[@]}"; do
    src="$tmp/standard/$s.mp3"
    if [ -f "$src" ]; then
        cp "$src" "$DEST/$s.mp3"
    else
        echo "warning: '$s.mp3' not found in archive" >&2
    fi
done

echo "Installed sounds into $DEST"
ls -1 "$DEST"
