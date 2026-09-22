#!/usr/bin/env bash
#
# Build a macOS application bundle: dist/OpenChess.app
# The binary lives in Contents/MacOS and assets in Contents/Resources, which the
# runtime resolves automatically (see src/paths.c).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
APP="$ROOT/dist/OpenChess.app"
ICONSET_ROOT=""
ICON_SRC="${ICON_SRC:-$ROOT/packaging/OpenChess.png}"
ICNS="$ROOT/dist/AppIcon.icns"

cleanup() { [ -n "$ICONSET_ROOT" ] && rm -rf "$ICONSET_ROOT"; }
trap cleanup EXIT

make -C "$ROOT" openchess

rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"

install -m 0755 "$ROOT/openchess" "$APP/Contents/MacOS/openchess"
install -m 0644 "$ROOT/packaging/Info.plist" "$APP/Contents/Info.plist"
install -m 0644 "$ROOT/chess.conf.example" "$APP/Contents/Resources/chess.conf.example"
rm -rf "$APP/Contents/Resources/assets"
cp -R "$ROOT/assets" "$APP/Contents/Resources/assets"

# Build AppIcon.icns from the source PNG (1024px and every @2x step).
if [ -f "$ICON_SRC" ] && command -v sips >/dev/null 2>&1 && command -v iconutil >/dev/null 2>&1; then
    ICONSET_ROOT="$(mktemp -d)"
    iconset="$ICONSET_ROOT/AppIcon.iconset"
    mkdir -p "$iconset"
    icns_size() { sips -z "$1" "$1" "$ICON_SRC" --out "$iconset/$2.png" >/dev/null; }
    icns_size 16   icon_16x16
    icns_size 32   icon_16x16@2x
    icns_size 32   icon_32x32
    icns_size 64   icon_32x32@2x
    icns_size 128  icon_128x128
    icns_size 256  icon_128x128@2x
    icns_size 256  icon_256x256
    icns_size 512  icon_256x256@2x
    icns_size 512  icon_512x512
    icns_size 1024 icon_512x512@2x
    iconutil -c icns "$iconset" -o "$ICNS"
    install -m 0644 "$ICNS" "$APP/Contents/Resources/AppIcon.icns"
else
    echo "warning: '$ICON_SRC' or iconutil missing; skipping app icon" >&2
fi

echo "Built $APP"
