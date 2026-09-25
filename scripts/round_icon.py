#!/usr/bin/env python3
"""Round the corners of the app icon (macOS-style squircle mask).

Usage: round_icon.py FILE [FILE ...]

Each file is rewritten in place with a transparent rounded-corner mask so the
square artwork reads as a macOS app icon. Requires Pillow.
"""
import sys

try:
    from PIL import Image, ImageDraw
except ImportError:  # pragma: no cover
    sys.exit("Pillow is required (pip install pillow)")

# macOS app-icon corner radius is about 22.37% of the side.
RADIUS_FRAC = 0.2237


def round_file(path):
    im = Image.open(path).convert("RGBA")
    w, h = im.size
    r = int(round(min(w, h) * RADIUS_FRAC))
    mask = Image.new("L", (w, h), 0)
    ImageDraw.Draw(mask).rounded_rectangle((0, 0, w - 1, h - 1), radius=r, fill=255)
    out = Image.new("RGBA", (w, h), (0, 0, 0, 0))
    out.paste(im, (0, 0), mask)
    out.save(path)
    print(f"rounded {path} ({w}x{h}, r={r})")


def main():
    if len(sys.argv) < 2:
        sys.exit(__doc__)
    for path in sys.argv[1:]:
        round_file(path)


if __name__ == "__main__":
    main()
