#!/bin/sh
# Stockfish 19, from the official release. Install privately, preserving GPL files.
set -eu
dest=$1
[ -x "$dest/stockfish" ] && exit 0
case "$(uname -s):$(uname -m)" in
    Darwin:*) name=stockfish-macos-universal; hash=a1f0e3bcc5a6927a11fe6fc8e54a779754645f3c2bae2cf13420fd1957adaa77 ;;
    Linux:x86_64) name=stockfish-linux-x86-64-universal; hash=9defc0d4e55d49c65a6d042f3e571a39fcea499ade6dbe741b53b8c65e03611f ;;
    Linux:aarch64|Linux:arm64) name=stockfish-linux-arm64-universal; hash=fe26cfd1d9db4c8af3d21e24d9ff34cacb31c1f940085a7583da11796f2bac01 ;;
    *) echo 'Unsupported Stockfish platform' >&2; exit 1 ;;
esac
mkdir -p "$(dirname "$dest")"
work=$(mktemp -d "$(dirname "$dest")/.stockfish-XXXXXX")
trap 'rm -rf "$work"' EXIT
trap 'exit 1' HUP INT TERM
curl --fail --location --proto '=https' --proto-redir '=https' --connect-timeout 15 --max-time 180 \
    "https://github.com/official-stockfish/Stockfish/releases/download/sf_19/$name.tar.gz" -o "$work/archive.tar.gz"
if command -v shasum >/dev/null 2>&1; then
    actual=$(shasum -a 256 "$work/archive.tar.gz" | cut -d ' ' -f 1)
else
    actual=$(sha256sum "$work/archive.tar.gz" | cut -d ' ' -f 1)
fi
[ "$actual" = "$hash" ] || { echo 'Stockfish checksum mismatch' >&2; exit 1; }
mkdir "$work/unpacked"
tar -xzf "$work/archive.tar.gz" -C "$work/unpacked"
binary=$(find "$work/unpacked" -type f -name "$name" | head -n 1)
[ -n "$binary" ] || { echo 'Stockfish executable missing' >&2; exit 1; }
root=$(dirname "$binary")
mv "$binary" "$root/stockfish"
chmod 755 "$root/stockfish"
# Only publish a complete installation; retain licence, source and network files.
[ ! -e "$dest" ] || { echo 'Stockfish destination already exists' >&2; exit 1; }
mv "$root" "$dest"
