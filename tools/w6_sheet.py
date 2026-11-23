#!/usr/bin/env python3
"""w6_sheet.py - screenshot contact sheets for the widescreen release (roadmap W6.2).

    python tools/w6_sheet.py <in_dir> <out_dir>

<in_dir> holds one folder per view width (534, 712, 716, 720, 1000) with the pictures that
`WIDETEST_COLS=<width> WIDETEST_PNG=<folder> widetest ... 3000` writes: <game>_3000_board.png
(the game's own 400 x 254 picture) and <game>_3000_view.png (the wide picture). For each game the
sheet shows every listed screen in three rows: Original 4:3 (black sides), Widescreen and
Stretch, each picture placed in the window's shape as compute_view (src/app/view.cpp) does with
Fit: as high as the window, 4:3 for the arcade's 400 columns. Plain scaling, no shader.

The pictures come from the owner's ROMs: keep them out of the repository.
"""
import os
import sys
from PIL import Image, ImageDraw

SCREENS = [  # (label, width, height, view columns)
    ("1920x1080", 1920, 1080, 534),
    ("2560x1440", 2560, 1440, 534),
    ("3840x2160", 3840, 2160, 534),
    ("2560x1080", 2560, 1080, 712),
    ("3440x1440", 3440, 1440, 716),
    ("3840x1600", 3840, 1600, 720),
    ("5120x1440", 5120, 1440, 1000),
]
GAMES = ["nbajam", "nbajamte", "nbajamse", "nbahangt", "nbamht"]
TILE_H = 120          # each window drawn this high on the sheet
GAP, LABEL = 10, 16


def place(win_w, win_h, pic, cols, mode):
    """the window (scaled to TILE_H high) with the picture in it"""
    k = TILE_H / win_h
    w = int(win_w * k + 0.5)
    tile = Image.new("RGB", (w, TILE_H), (0, 0, 0))
    if mode == "stretch":
        tile.paste(pic.resize((w, TILE_H), Image.BILINEAR), (0, 0))
        return tile
    # 400 game columns are 4:3 of the height; a wider picture is wider in the same proportion
    pw = int(TILE_H * 4 / 3 * cols / 400 + 0.5)
    if pw > w:   # never wider than the window
        pw = w
    tile.paste(pic.resize((pw, TILE_H), Image.BILINEAR), ((w - pw) // 2, 0))
    return tile


def main():
    if len(sys.argv) != 3:
        print(__doc__)
        return 2
    src, out = sys.argv[1], sys.argv[2]
    os.makedirs(out, exist_ok=True)
    for g in GAMES:
        rows = []
        for mode in ("original", "widescreen", "stretch"):
            tiles = []
            for label, ww, wh, cols in SCREENS:
                board = os.path.join(src, str(cols), f"{g}_3000_board.png")
                view = os.path.join(src, str(cols), f"{g}_3000_view.png")
                if not os.path.exists(board):
                    continue
                if mode == "widescreen" and os.path.exists(view):
                    tiles.append((label, place(ww, wh, Image.open(view).convert("RGB"), cols, mode)))
                else:
                    tiles.append((label, place(ww, wh, Image.open(board).convert("RGB"), 400, mode)))
            rows.append((mode, tiles))
        width = max(sum(t.width for _, t in tiles) + GAP * (len(tiles) + 1) for _, tiles in rows)
        height = len(rows) * (TILE_H + LABEL + GAP) + GAP + LABEL
        sheet = Image.new("RGB", (width, height), (40, 40, 48))
        d = ImageDraw.Draw(sheet)
        d.text((GAP, 2), f"{g}: Original 4:3 / Widescreen / Stretch, frame 3000 of a scripted game", fill=(255, 255, 255))
        y = LABEL + GAP
        for mode, tiles in rows:
            x = GAP
            for label, t in tiles:
                d.text((x, y), f"{label} {mode}", fill=(220, 220, 220))
                sheet.paste(t, (x, y + LABEL))
                x += t.width + GAP
            y += TILE_H + LABEL + GAP
        path = os.path.join(out, f"{g}_matrix.png")
        sheet.save(path)
        print("wrote", path)
    return 0


if __name__ == "__main__":
    sys.exit(main())
