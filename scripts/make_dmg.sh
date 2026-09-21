#!/usr/bin/env bash
#
# Build dist/OpenChess.dmg containing OpenChess.app (macOS only).
# The app is unsigned, so Gatekeeper may warn on first launch; open it via
# right-click > Open, or run:  xattr -dr com.apple.quarantine /Applications/OpenChess.app
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
APP="$ROOT/dist/OpenChess.app"
DMG="$ROOT/dist/OpenChess.dmg"

command -v hdiutil >/dev/null 2>&1 || { echo "error: hdiutil not found (macOS only)" >&2; exit 1; }

"$ROOT/scripts/make_app.sh"

rm -f "$DMG"
hdiutil create -volname "OpenChess" -srcfolder "$APP" -ov -format UDZO "$DMG"

echo "Created $DMG"
