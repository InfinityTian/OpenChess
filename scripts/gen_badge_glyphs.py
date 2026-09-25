#!/usr/bin/env python3
"""Generate path-based move-quality badge SVGs.

nanosvg (used by SDL_image) ignores <text>, so badge glyphs must be outlines.
This script extracts the '?' and '!' outlines from a bold font and emits the
badges under assets/badges/. Run once; the generated SVGs are committed.

Usage: gen_badge_glyphs.py [FONT]

Default font: /System/Library/Fonts/Supplemental/Arial Bold.ttf
"""
import os
import sys

from fontTools.ttLib import TTFont
from fontTools.pens.svgPathPen import SVGPathPen
from fontTools.pens.transformPen import TransformPen
from fontTools.misc.transform import Transform

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
OUT = os.path.join(ROOT, "assets", "badges")
DEFAULT_FONT = "/System/Library/Fonts/Supplemental/Arial Bold.ttf"


def glyph_path(font, glyphset, ch, size, baseline, cx, letter_spacing):
    """Return (path_d, advance) for `ch` scaled to `size`, baseline at y."""
    upm = font["head"].unitsPerEm
    scale = size / upm
    gname = font.getBestCmap()[ord(ch)]
    glyph = glyphset[gname]
    pen = SVGPathPen(glyphset)
    t = Transform(scale, 0, 0, -scale, cx, baseline)
    glyph.draw(TransformPen(pen, t))
    adv = glyph.width * scale + letter_spacing
    return pen.getCommands(), adv


def text_path(font, glyphset, text, size, baseline, letter_spacing):
    upm = font["head"].unitsPerEm
    scale = size / upm
    widths = []
    for ch in text:
        gname = font.getBestCmap()[ord(ch)]
        widths.append(glyphset[gname].width * scale)
    total = sum(widths) + letter_spacing * (len(text) - 1)
    x = 64.0 - total / 2.0
    parts = []
    for ch, w in zip(text, widths):
        d, _ = glyph_path(font, glyphset, ch, size, baseline, x, letter_spacing)
        parts.append(d)
        x += w + letter_spacing
    return " ".join(parts)


BADGES = {
    "badge_inaccuracy.svg": ("#F7C631", "?!", 57, 84, -4),
    "badge_mistake.svg":    ("#FFA459", "?",  69, 88, 0),
    "badge_blunder.svg":    ("#FA412D", "??", 61, 84, -7),
    "badge_interesting.svg":("#7775A4", "!?", 57, 84, -4),
    "badge_great.svg":      ("#7499BF", "!",  69, 88, 0),
    "badge_brilliant.svg":  ("#27C2A3", "!!", 57, 84, -4),
}

TEMPLATE = """<svg xmlns="http://www.w3.org/2000/svg" width="128" height="128" viewBox="0 0 128 128">
  <title>{title}</title>
  <circle cx="64" cy="64" r="48" fill="{color}"/>
  <path fill="#FFFFFF" d="{d}"/>
</svg>
"""

THUMB = """<svg xmlns="http://www.w3.org/2000/svg" width="128" height="128" viewBox="0 0 128 128">
  <title>Excellent</title>
  <circle cx="64" cy="64" r="48" fill="#95BB4A"/>
  <path fill="#FFFFFF" d="M52 96 L52 58 C52 58 50 52 50 46 C50 38 56 34 61 38 C65 41 66 47 66 52 L66 56 L82 56 C87 56 90 59 89 64 L85 88 C84 93 80 96 75 96 Z M38 58 L46 58 L46 96 L38 96 C35 96 33 94 33 91 L33 63 C33 60 35 58 38 58 Z"/>
</svg>
"""


def main():
    font_path = sys.argv[1] if len(sys.argv) > 1 else DEFAULT_FONT
    font = TTFont(font_path)
    glyphset = font.getGlyphSet()
    for name, (color, text, size, baseline, ls) in BADGES.items():
        d = text_path(font, glyphset, text, size, baseline, ls)
        svg = TEMPLATE.format(title=text, color=color, d=d)
        with open(os.path.join(OUT, name), "w") as f:
            f.write(svg)
        print("wrote", name)
    with open(os.path.join(OUT, "badge_excellent.svg"), "w") as f:
        f.write(THUMB)
    print("wrote badge_excellent.svg")


if __name__ == "__main__":
    main()
