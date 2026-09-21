#!/usr/bin/env bash
#
# Build a macOS application bundle: dist/OpenChess.app
# The binary lives in Contents/MacOS and assets in Contents/Resources, which the
# runtime resolves automatically (see src/paths.c).
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
APP="$ROOT/dist/OpenChess.app"

make -C "$ROOT" openchess

rm -rf "$APP"
mkdir -p "$APP/Contents/MacOS" "$APP/Contents/Resources"

install -m 0755 "$ROOT/openchess" "$APP/Contents/MacOS/openchess"
install -m 0644 "$ROOT/packaging/Info.plist" "$APP/Contents/Info.plist"
install -m 0644 "$ROOT/chess.conf.example" "$APP/Contents/Resources/chess.conf.example"
rm -rf "$APP/Contents/Resources/assets"
cp -R "$ROOT/assets" "$APP/Contents/Resources/assets"

echo "Built $APP"
