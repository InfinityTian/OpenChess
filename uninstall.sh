#!/usr/bin/env bash
#
# Remove an OpenChess install created by install.sh.
#
#   ./uninstall.sh              remove program + assets
#   ./uninstall.sh --purge      also remove ~/.config/openchess (settings)
#
# Use the same PREFIX you installed with:  PREFIX=/usr/local ./uninstall.sh
set -euo pipefail

PREFIX="${PREFIX:-$HOME/.local}"
DEST_BIN="$PREFIX/bin/openchess"
DEST_SHARE="$PREFIX/share/openchess"
CONF_DIR="${XDG_CONFIG_HOME:-$HOME/.config}/openchess"

rm -f "$DEST_BIN"
rm -rf "$DEST_SHARE"

if [ "${1:-}" = "--purge" ]; then
    rm -rf "$CONF_DIR"
    echo "Removed settings: $CONF_DIR"
fi

# Clean up the share directory if it is now empty.
rmdir "$PREFIX/share/openchess" 2>/dev/null || true

echo "Uninstalled OpenChess from $PREFIX"
