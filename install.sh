#!/usr/bin/env bash
#
# Install OpenChess into $PREFIX (default: ~/.local) for terminal users on
# Linux and macOS. Builds the binary, then installs it plus assets and the
# example config:
#
#   $PREFIX/bin/openchess
#   $PREFIX/share/openchess/assets/
#   $PREFIX/share/openchess/chess.conf.example
#
# Override the location with:  PREFIX=/usr/local ./install.sh
set -euo pipefail

ROOT="$(cd "$(dirname "$0")" && pwd)"
PREFIX="${PREFIX:-$HOME/.local}"
DEST_BIN="$PREFIX/bin"
DEST_SHARE="$PREFIX/share/openchess"

command -v make >/dev/null 2>&1 || { echo "error: 'make' is required" >&2; exit 1; }
command -v pkg-config >/dev/null 2>&1 || { echo "error: 'pkg-config' is required" >&2; exit 1; }

echo "Building OpenChess ..."
make -C "$ROOT" openchess

echo "Installing to $PREFIX ..."
mkdir -p "$DEST_BIN" "$DEST_SHARE"

# install(1) is available on macOS and Linux; fall back to cp/rm otherwise.
if command -v install >/dev/null 2>&1; then
    install -m 0755 "$ROOT/openchess" "$DEST_BIN/openchess"
    install -m 0644 "$ROOT/chess.conf.example" "$DEST_SHARE/chess.conf.example"
else
    cp "$ROOT/openchess" "$DEST_BIN/openchess"
    chmod 0755 "$DEST_BIN/openchess"
    cp "$ROOT/chess.conf.example" "$DEST_SHARE/chess.conf.example"
fi

rm -rf "$DEST_SHARE/assets"
cp -R "$ROOT/assets" "$DEST_SHARE/assets"

echo "Installed: $DEST_BIN/openchess"
echo "Assets:    $DEST_SHARE/assets"

case ":$PATH:" in
    *":$DEST_BIN:"*) ;;
    *) echo
       echo "Add $DEST_BIN to your PATH, e.g.:"
       echo "    echo 'export PATH=\"$DEST_BIN:\$PATH\"' >> ~/.zshrc   # or ~/.bashrc" ;;
esac
