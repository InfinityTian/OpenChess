#!/usr/bin/env bash
#
# Build dist/OpenChess.dmg: a drag-to-install disk image with OpenChess.app, an
# "Applications" shortcut and an arrow background. Built as HFS+ so Finder can
# store the window layout.
#
# The app is unsigned and not notarized, so Gatekeeper may block it on the
# receiving Mac. If Finder cannot lay out the window (headless build, or
# Automation permission not granted) the script warns and still emits a valid
# image. On the receiving Mac, if the app will not open, run:
#   sudo xattr -r -d com.apple.quarantine /path/to/OpenChess.dmg
# or, after dragging to /Applications:
#   sudo xattr -r -d com.apple.quarantine /Applications/OpenChess.app
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
APP="$ROOT/dist/OpenChess.app"
DMG="$ROOT/dist/OpenChess.dmg"
RW="$ROOT/dist/OpenChess-rw.dmg"
STAGE="$ROOT/dist/dmg"
ICNS="$ROOT/dist/AppIcon.icns"
BG_SCRIPT="$ROOT/scripts/make_dmg_background.py"
VOL="OpenChess"

command -v hdiutil >/dev/null 2>&1 || { echo "error: hdiutil not found (macOS only)" >&2; exit 1; }

"$ROOT/scripts/make_app.sh"

rm -rf "$STAGE" "$RW" "$DMG"
mkdir -p "$STAGE/.background"

# --- stage the volume: app + Applications shortcut + arrow background ---
cp -R "$APP" "$STAGE/OpenChess.app"
ln -s /Applications "$STAGE/Applications"
if command -v python3 >/dev/null 2>&1; then
    python3 "$BG_SCRIPT" "$STAGE/.background/background.png" >/dev/null
else
    echo "warning: python3 not found; DMG will have no arrow background" >&2
fi

# Strip quarantine from the local copy (recipients re-add it on download).
xattr -dr com.apple.quarantine "$STAGE/OpenChess.app" 2>/dev/null || true

# --- create + mount a writable HFS+ image ---
hdiutil detach "/Volumes/$VOL" >/dev/null 2>&1 || true
hdiutil create -volname "$VOL" -srcfolder "$STAGE" -ov -format UDRW -fs HFS+ "$RW" >/dev/null
MOUNT_DIR="$(hdiutil attach -readwrite -noverify -noautoopen "$RW" | grep -o '/Volumes/.*' | tail -1)"
[ -n "$MOUNT_DIR" ] || { echo "error: could not mount $RW" >&2; exit 1; }

# --- arrange the window/icons with Finder (best effort) ---
osascript <<OSA || echo "warning: Finder layout skipped (permission/headless); image still usable" >&2
tell application "Finder"
    tell disk "$VOL"
        open
        set current view of container window to icon view
        set toolbar visible of container window to false
        set statusbar visible of container window to false
        set the bounds of container window to {200, 200, 860, 620}
        set vopts to the icon view options of container window
        set arrangement of vopts to not arranged
        set icon size of vopts to 128
        set text size of vopts to 13
        set background picture of vopts to file ".background:background.png"
        set position of item "OpenChess.app" of container window to {165, 190}
        set position of item "Applications" of container window to {495, 190}
        update without registering applications
        delay 1
        close
    end tell
end tell
OSA

# Set the volume icon only after Finder has laid out the window: Finder drops a
# .VolumeIcon.icns that already exists when it rewrites the volume's view state.
if [ -f "$ICNS" ] && command -v SetFile >/dev/null 2>&1; then
    cp "$ICNS" "$MOUNT_DIR/.VolumeIcon.icns"
    SetFile -a C "$MOUNT_DIR" 2>/dev/null || true
fi

sync
hdiutil detach "$MOUNT_DIR" >/dev/null || hdiutil detach "$MOUNT_DIR" -force >/dev/null

# --- compress and clean up ---
hdiutil convert "$RW" -format UDZO -o "$DMG" >/dev/null
rm -f "$RW"
rm -rf "$STAGE"

xattr -dr com.apple.quarantine "$DMG" 2>/dev/null || true

echo "Created $DMG"
echo
echo "The app is unsigned. On the receiving Mac, if Gatekeeper blocks it, run:"
echo "  sudo xattr -r -d com.apple.quarantine $DMG"
echo "  # or, after dragging to /Applications:"
echo "  sudo xattr -r -d com.apple.quarantine /Applications/OpenChess.app"
