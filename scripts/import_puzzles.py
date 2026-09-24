#!/usr/bin/env python3
"""Import a small Lichess puzzle subset for OpenChess' Puzzles mode.

Sources
-------
  hf (default)   stream https://huggingface.co/datasets/Lichess/chess-puzzles-with-games
                 (needs:  pip install huggingface_hub pyarrow)
  csv <file>     the official Lichess puzzle CSV, optionally .zst
                 (needs:  pip install zstandard  for .zst)

The output is newline-delimited JSON:
  {"id","fen","moves","rating","themes":[...]}

Examples
--------
  # 20000 puzzles, any theme, ratings 800..2400
  scripts/import_puzzles.py --count 20000 --min-rating 800 --max-rating 2400

  # balanced sample across classes (500 each)
  scripts/import_puzzles.py --themes mateIn1,mateIn2,fork,pin,discoveredAttack,\
deflection,attraction,sacrifice,defensiveMove,kingsideAttack,endgame --per-theme 500

  # from a downloaded lichess_db_puzzle.csv.zst
  scripts/import_puzzles.py --source csv lichess_db_puzzle.csv.zst --count 20000
"""
import argparse
import csv
import io
import json
import os
import random
import sys

DEFAULT_THEMES = [
    "mateIn1", "mateIn2", "mateIn3", "fork", "pin", "skewer",
    "discoveredAttack", "deflection", "attraction", "sacrifice",
    "defensiveMove", "kingsideAttack", "queensideAttack", "endgame",
    "hangingPiece", "trappedPiece", "intermezzo", "quietMove",
]


def default_out():
    xdg = os.environ.get("XDG_DATA_HOME")
    if xdg:
        base = os.path.join(xdg, "openchess")
    elif os.name == "nt" and os.environ.get("APPDATA"):
        base = os.path.join(os.environ["APPDATA"], "openchess")
    else:
        base = os.path.join(os.path.expanduser("~"), ".local", "share", "openchess")
    return os.path.join(base, "puzzles", "puzzles.jsonl")


def norm_row(puzzle_id, fen, moves, rating, themes):
    if not puzzle_id or not fen or not moves:
        return None
    if isinstance(themes, list):
        tl = [str(t) for t in themes if t]
    else:
        tl = str(themes or "").split()
    return {
        "id": str(puzzle_id),
        "fen": str(fen),
        "moves": str(moves),
        "rating": int(rating) if rating not in (None, "") else 1500,
        "themes": tl,
    }


def iter_csv(path):
    opener = open
    if path.endswith(".zst"):
        try:
            import zstandard as zstd
        except ImportError:
            sys.exit("error: reading .zst needs 'pip install zstandard'")
        fh = open(path, "rb")
        dctx = zstd.ZstdDecompressor()
        stream = io.TextIOWrapper(dctx.stream_reader(fh), encoding="utf-8")
    else:
        stream = opener(path, "r", encoding="utf-8", newline="")
    reader = csv.DictReader(stream)
    for row in reader:
        yield norm_row(row.get("PuzzleId"), row.get("FEN"), row.get("Moves"),
                       row.get("Rating"), row.get("Themes"))


def iter_hf():
    """Stream the auto-converted Parquet shards directly (no `datasets`).

    The HF `datasets` loader fails on this dataset's `json`/`timestamp` columns,
    so we read the Parquet conversion with huggingface_hub + pyarrow instead,
    selecting only the puzzle fields.
    """
    try:
        from huggingface_hub import HfApi, HfFileSystem
        import pyarrow.parquet as pq
    except ImportError:
        sys.exit("error: the HF source needs 'pip install huggingface_hub pyarrow'")

    repo = "Lichess/chess-puzzles-with-games"
    rev = "refs/convert/parquet"
    files = [f for f in HfApi().list_repo_files(repo, repo_type="dataset", revision=rev)
             if f.endswith(".parquet")]
    cols = ["PuzzleId", "FEN", "Moves", "Rating", "Themes"]
    fs = HfFileSystem()
    for f in files:
        # Range reads: only the Parquet footer + needed row groups are fetched.
        with fs.open(f"datasets/{repo}/{f}", "rb", revision=rev) as fh:
            pf = pq.ParquetFile(fh)
            for batch in pf.iter_batches(batch_size=8192, columns=cols):
                for row in batch.to_pylist():
                    yield norm_row(row.get("PuzzleId"), row.get("FEN"),
                                   row.get("Moves"), row.get("Rating"),
                                   row.get("Themes"))


def main():
    ap = argparse.ArgumentParser(description=__doc__,
                                 formatter_class=argparse.RawDescriptionHelpFormatter)
    ap.add_argument("--source", nargs="+", default=["hf"],
                    help="'hf' or 'csv <file>'")
    ap.add_argument("--out", default=default_out())
    ap.add_argument("--count", type=int, default=20000)
    ap.add_argument("--min-rating", type=int, default=600)
    ap.add_argument("--max-rating", type=int, default=3000)
    ap.add_argument("--themes", default="",
                    help="comma-separated themes (empty = any)")
    ap.add_argument("--per-theme", type=int, default=0,
                    help="if >0, take this many per theme (balanced)")
    ap.add_argument("--seed", type=int, default=None)
    ap.add_argument("--max-scan", type=int, default=0,
                    help="stop after scanning this many rows (0 = no limit)")
    args = ap.parse_args()

    if args.seed is not None:
        random.seed(args.seed)

    want = [t.strip() for t in args.themes.split(",") if t.strip()]

    if args.source[0] == "hf":
        source = iter_hf()
    elif args.source[0] == "csv":
        if len(args.source) < 2:
            sys.exit("error: --source csv needs a file path")
        source = iter_csv(args.source[1])
    else:
        sys.exit("error: unknown source %r" % args.source[0])

    targets = {}
    if want and args.per_theme > 0:
        for t in want:
            targets[t] = args.per_theme
        needed = sum(targets.values())
    else:
        needed = args.count

    os.makedirs(os.path.dirname(args.out) or ".", exist_ok=True)
    seen = set()
    written = 0
    scanned = 0

    with open(args.out, "w", encoding="utf-8") as out:
        for obj in source:
            scanned += 1
            if obj is None:
                continue
            r = obj["rating"]
            if r < args.min_rating or r > args.max_rating:
                continue
            if want:
                cand = [t for t in want
                        if obj["themes"] and t in obj["themes"]
                        and (not targets or targets.get(t, 0) > 0)]
                if not cand:
                    continue
                credited = cand[0] if args.per_theme > 0 else random.choice(cand)
            else:
                credited = None

            if obj["id"] in seen:
                continue
            seen.add(obj["id"])
            out.write(json.dumps(obj, ensure_ascii=False) + "\n")
            written += 1
            if targets:
                targets[credited] -= 1

            if written >= needed:
                break
            if args.max_scan and scanned >= args.max_scan:
                break
            if scanned % 50000 == 0:
                sys.stderr.write("\rscanned %d, wrote %d ..." % (scanned, written))
                sys.stderr.flush()

    sys.stderr.write("\n")
    print("Wrote %d puzzles to %s (scanned %d)" % (written, args.out, scanned))
    if targets:
        short = {t: n for t, n in targets.items() if n > 0}
        if short:
            print("Themes not fully filled: %s" % short)


if __name__ == "__main__":
    main()
