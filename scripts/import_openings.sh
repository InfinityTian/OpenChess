#!/usr/bin/env bash
#
# Download the lichess opening book (CC0) and concatenate it into
# assets/openings.tsv for the review's "Book" classification.
# Source: https://github.com/lichess-org/chess-openings
set -euo pipefail

ROOT="$(cd "$(dirname "$0")/.." && pwd)"
OUT="$ROOT/assets/openings.tsv"
BASE="https://raw.githubusercontent.com/lichess-org/chess-openings/master"

tmp="$(mktemp)"
: > "$tmp"
for f in a b c d e; do
    echo "fetching $f.tsv ..."
    curl -fsSL "$BASE/$f.tsv" >> "$tmp"
done
mv "$tmp" "$OUT"
echo "Wrote $(wc -l < "$OUT") openings to $OUT"
