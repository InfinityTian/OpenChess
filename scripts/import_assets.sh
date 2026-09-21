#!/usr/bin/env bash
#
# Import all boards and piece sets from the chess.com asset collection and
# (re)generate assets/themes.txt.
#
#   scripts/import_assets.sh [git-url] [branch]
#
# Defaults to GiorgioMegrelli/chess.com-boards-and-pieces (branch v2.x).
# The images are (c) chess.com; see ATTRIBUTIONS.md.
set -euo pipefail

REPO="${1:-https://github.com/GiorgioMegrelli/chess.com-boards-and-pieces.git}"
BRANCH="${2:-v2.x}"
ROOT="$(cd "$(dirname "$0")/.." && pwd)"
DEST="$ROOT/assets"

TMP="$(mktemp -d)"
trap 'rm -rf "$TMP"' EXIT

echo "Cloning $REPO ($BRANCH) ..."
git clone --depth 1 --branch "$BRANCH" "$REPO" "$TMP/src"

mkdir -p "$DEST/boards" "$DEST/pieces"
cp "$TMP/src/boards/"*.png "$DEST/boards/"
cp -R "$TMP/src/pieces/." "$DEST/pieces/"

python3 - "$TMP/src/default-config.json" "$DEST/themes.txt" <<'PY'
import json, sys
cfg = json.load(open(sys.argv[1]))
out = ["# OpenChess theme manifest",
       "# board <key> <Label>  /  piece <key> <Label>",
       "# Labels derived from GiorgioMegrelli/chess.com-boards-and-pieces.",
       "# Assets are (c) chess.com; used here for personal/educational purposes."]
for section, kind in (("boards", "board"), ("pieces", "piece")):
    for key, meta in cfg.get(section, {}).get("collections", {}).items():
        out.append(f"{kind} {key} {meta.get('readable_name') or key}")
open(sys.argv[2], "w").write("\n".join(out) + "\n")
PY

echo "Done. $(ls "$DEST/boards" | wc -l | tr -d ' ') boards, $(ls -d "$DEST/pieces"/*/ | wc -l | tr -d ' ') piece sets."
