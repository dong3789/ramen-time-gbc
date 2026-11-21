#!/usr/bin/env python3
"""Draw every RAMEN TIME PNG, then convert it to GBDK C with tools/png2gbc.py."""
from __future__ import annotations

import argparse
import os
import subprocess
import sys

from PIL import Image, ImageDraw

HERE = os.path.dirname(os.path.abspath(__file__))
ROOT = os.path.dirname(HERE)
GEN = os.path.join(ROOT, "src", "gen")
TOOLS = os.path.join(ROOT, "tools")
PNG2GBC = os.path.join(TOOLS, "png2gbc.py")

sys.path.insert(0, TOOLS)
from artfont import draw_text, draw_text_centered, text_size  # noqa: E402

# ---------------------------------------------------------------- palettes (hex, no # in --palette flags)
INK = "#2A140C"
CREAM = "#FFE7B8"
RED = "#E33A2A"
GOLD = "#F6C14B"
WOOD = "#B56A2E"
BROTH = "#C45A1A"
NOODLE = "#E8C25A"
GREENZ = "#3ECF6A"
STEEL = "#6A5344"  # unused in 4-colour sets; kept for comments

TITLE_PAL = f"{INK[1:]},{CREAM[1:]},{RED[1:]},{GOLD[1:]}"
PLAY_PAL = f"{INK[1:]},{CREAM[1:]},{WOOD[1:]},{RED[1:]}"
POT_PAL = f"{INK[1:]},{CREAM[1:]},{NOODLE[1:]},{BROTH[1:]}"
FONT_PAL = f"{CREAM[1:]},{INK[1:]},{RED[1:]},{GOLD[1:]}"
GAUGE_PAL = f"{INK[1:]},{CREAM[1:]},{GREENZ[1:]},{GOLD[1:]}"
# sprites: 3 drawable colours, transparency is index 0
CURSOR_COLS = [GOLD, RED, INK]
EGG_COLS = ["#FFF6E0", "#F0A020", INK]
ONION_COLS = ["#3DA84A", "#B6E07A", INK]

TILE = 8
SCREEN_W, SCREEN_H = 160, 144

# screen regions (tiles) — also written to layout.h
PRESS_REGION = (1, 14, 18, 2)       # title "PRESS START" band
POT_REGION = (7, 5, 6, 5)           # playfield pot interior
GAUGE_REGION = (3, 15, 14, 1)       # 112 px bar, perfect = tiles 6-7 of the bar
PROMPT_REGION = (1, 13, 18, 1)
JUDGE_REGION = (4, 14, 12, 1)
HUD_ROW = 0

POT_STATES = (
    "WATER", "BUBBLE0", "BUBBLE1", "NOODLES",
    "SOUP", "PERFECT", "HARD", "SOGGY",
)
POT_W, POT_H = 6, 5  # tiles
FONT_CHARS = " 0123456789ABCDEFGHIJKLMNOPQRSTUVWXYZ!/:-"


def rgb(h: str):
    h = h.lstrip("#")
    return tuple(int(h[i : i + 2], 16) for i in (0, 2, 4))


def new_rgb(size, color):
    return Image.new("RGB", size, rgb(color))


# ---------------------------------------------------------------- title
def make_title(path):
    img = new_rgb((SCREEN_W, SCREEN_H), CREAM)
    d = ImageDraw.Draw(img)
    ink, cream, red, gold = rgb(INK), rgb(CREAM), rgb(RED), rgb(GOLD)

    # wallpaper: tiny steam dots at repeatable in-tile offsets (tile-friendly)
    for ty in range(18):
        for tx in range(20):
            px, py = tx * 8, ty * 8
            if (tx + ty) % 3 == 0:
                d.point((px + 2, py + 3), fill=gold)
            if (tx + 2 * ty) % 4 == 0:
                d.point((px + 5, py + 6), fill=red)

    # top red banner
    d.rectangle([0, 0, 159, 31], fill=red)
    d.rectangle([0, 31, 159, 32], fill=ink)
    tw, _ = text_size("RAMEN TIME", 2)
    draw_text(img, (SCREEN_W - tw) // 2 + 1, 9, "RAMEN TIME", ink, scale=2)
    draw_text(img, (SCREEN_W - tw) // 2, 8, "RAMEN TIME", gold, scale=2)

    # bowl body
    # outer bowl (red), rim (gold), broth (red-dark via ink stipple + gold noodles)
    d.ellipse([28, 46, 131, 132], fill=red, outline=ink)
    d.ellipse([36, 52, 123, 108], fill=gold, outline=ink)  # rim lip
    d.ellipse([42, 58, 117, 104], fill=red)  # broth
    # broth depth
    for x in range(46, 114, 3):
        d.point((x, 96), fill=ink)
        d.point((x + 1, 97), fill=ink)
    # noodles in the bowl (gold waves)
    for i, y in enumerate((70, 76, 82, 88)):
        for x in range(50, 110):
            yy = y + (2 if (x // 6 + i) % 2 == 0 else -1)
            d.point((x, yy), fill=gold)
            d.point((x, yy + 1), fill=gold)
    # egg half
    d.ellipse([78, 68, 98, 86], fill=cream, outline=ink)
    d.ellipse([84, 72, 94, 82], fill=gold)
    # green-onion stand-in: short gold/ink strokes (green isn't in this palette)
    for x, y in ((58, 74), (60, 76), (62, 73), (100, 80), (102, 78)):
        d.line([x, y, x + 4, y - 6], fill=ink)
        d.line([x + 1, y, x + 5, y - 6], fill=cream)

    # chopsticks
    d.line([18, 64, 70, 58], fill=ink, width=2)
    d.line([20, 70, 72, 62], fill=ink, width=2)

    # steam curls above the bowl, under the banner
    for cx, cy, s in ((60, 40, 1), (80, 36, 1), (100, 40, 1), (70, 44, 0), (90, 44, 0)):
        d.arc([cx, cy, cx + 10, cy + 10], 200, 80, fill=ink)
        if s:
            d.arc([cx + 4, cy - 6, cx + 12, cy + 4], 200, 80, fill=ink)

    # PRESS START band (tile rows 14-15) — keep it tile-aligned for blinking
    px, py, pw, ph = PRESS_REGION
    d.rectangle([px * 8, py * 8, (px + pw) * 8 - 1, (py + ph) * 8 - 1], fill=red)
    d.rectangle([px * 8, py * 8, (px + pw) * 8 - 1, (py + ph) * 8 - 1], outline=gold)
    tw, th = text_size("PRESS START", 1)
    tx = px * 8 + ((pw * 8) - tw) // 2
    ty = py * 8 + ((ph * 8) - th) // 2
    draw_text(img, tx + 1, ty + 1, "PRESS START", ink, scale=1)
    draw_text(img, tx, ty, "PRESS START", gold, scale=1)

    img.save(path)
    return path


# ---------------------------------------------------------------- play background
def make_play(path):
    img = new_rgb((SCREEN_W, SCREEN_H), CREAM)
    d = ImageDraw.Draw(img)
    ink, cream, wood, red = rgb(INK), rgb(CREAM), rgb(WOOD), rgb(RED)

    # wall (rows 1-11-ish) with a simple tileable diamond
    d.rectangle([0, 8, 159, 95], fill=cream)
    for ty in range(1, 12):
        for tx in range(20):
            px, py = tx * 8, ty * 8
            if (tx + ty) % 2 == 0:
                d.point((px + 3, py + 3), fill=wood)
                d.point((px + 4, py + 3), fill=wood)

    # backsplash stripe
    d.rectangle([0, 88, 159, 95], fill=wood)
    d.rectangle([0, 86, 159, 87], fill=ink)

    # counter
    d.rectangle([0, 96, 159, 111], fill=wood)
    d.rectangle([0, 96, 159, 97], fill=ink)
    for x in range(0, 160, 16):
        d.line([x, 98, x, 110], fill=ink)

    # HUD row
    d.rectangle([0, 0, 159, 7], fill=cream)
    d.line([0, 7, 159, 7], fill=ink)

    # prompt + judge + hint rows (13, 14, 16) stay cream
    d.rectangle([0, 104, 159, 143], fill=cream)  # will be overwritten: wait, counter is 96-111
    # redo lower area more carefully:
    # row 12 (96-103): still counter front
    # row 13 (104-111): prompt cream? That's tight with counter.
    # Layout: counter uses rows 11-12 (88-103). Prompt row 13 (104-111).
    # I already drew wood 96-111. Fix prompt rows.
    d.rectangle([0, 104, 159, 143], fill=cream)
    d.line([0, 104, 159, 104], fill=ink)

    # gauge row (15) dark tray
    gy = GAUGE_REGION[1] * 8
    d.rectangle([0, gy, 159, gy + 7], fill=ink)

    # hint row 16 already cream; row 17 a red kickboard
    d.rectangle([0, 136, 159, 143], fill=red)

    # --- pot on the counter (rim only; interior is overwritten in-game) ---
    # interior is tiles (7,5)-(12,9) = px 56-103, py 40-79
    # rim around it: slightly larger ellipse / rounded pot
    d.ellipse([44, 32, 115, 96], fill=red, outline=ink)
    # handles
    d.rectangle([36, 56, 46, 68], fill=red, outline=ink)
    d.rectangle([113, 56, 123, 68], fill=red, outline=ink)
    # inner lip (gold not available — use cream)
    d.ellipse([52, 38, 107, 86], fill=cream, outline=ink)
    # placeholder interior (will be replaced); keep it ink so leftover tiles are dark
    ix, iy, iw, ih = POT_REGION
    d.rectangle([ix * 8, iy * 8, (ix + iw) * 8 - 1, (iy + ih) * 8 - 1], fill=ink)

    # a little steam hint above the pot (static, cute)
    for cx, cy in ((68, 22), (80, 18), (92, 22)):
        d.arc([cx, cy, cx + 8, cy + 8], 200, 80, fill=ink)

    img.save(path)
    return path


# ---------------------------------------------------------------- pot interiors (8 states, shared 4 colours)
def _fill_broth(d, color, ripple=None):
    d.rectangle([0, 0, 47, 39], fill=rgb(color))
    if ripple:
        for x in range(2, 46, 4):
            d.point((x, 34), fill=rgb(ripple))
            d.point((x + 1, 35), fill=rgb(ripple))


def _bubbles(d, offset):
    bubbles = [(6, 10, 3), (18, 16, 4), (30, 8, 3), (10, 24, 2), (28, 26, 3), (22, 6, 2)]
    for x, y, r in bubbles:
        xx = (x + offset) % 44 + 2
        yy = y
        d.ellipse([xx, yy, xx + r, yy + r], outline=rgb(INK), fill=rgb(CREAM))


def _noodles(d, wavy, pale=False):
    col = rgb(NOODLE)
    dark = rgb(INK)
    for i, y0 in enumerate((8, 14, 20, 26)):
        for x in range(4, 44):
            if wavy:
                y = y0 + (2 if ((x // 5) + i) % 2 == 0 else -1)
            else:
                y = y0 + (1 if i % 2 else 0)
            if 0 <= y < 40:
                d.point((x, y), fill=col)
                d.point((x, y + 1), fill=col if not pale else dark)


def _steam(d):
    for cx, cy in ((10, 2), (22, 0), (34, 2)):
        d.arc([cx, cy, cx + 8, cy + 8], 200, 90, fill=rgb(CREAM))


def make_pot_state(kind):
    img = new_rgb((POT_W * 8, POT_H * 8), CREAM)
    d = ImageDraw.Draw(img)
    if kind == "WATER":
        _fill_broth(d, CREAM, INK)
        # horizontal ripples (avoid a "face")
        for y in (8, 16, 24, 32):
            for x in range(4, 44, 5):
                d.point((x, y), fill=rgb(INK))
                d.point((x + 1, y), fill=rgb(INK))
                d.point((x + 2, y + 1), fill=rgb(INK))
    elif kind == "BUBBLE0":
        _fill_broth(d, CREAM, INK)
        _bubbles(d, 0)
    elif kind == "BUBBLE1":
        _fill_broth(d, CREAM, INK)
        _bubbles(d, 8)
    elif kind == "NOODLES":
        _fill_broth(d, CREAM, INK)
        _noodles(d, wavy=False, pale=True)
    elif kind == "SOUP":
        _fill_broth(d, BROTH, INK)
        _noodles(d, wavy=True)
    elif kind == "PERFECT":
        _fill_broth(d, BROTH, CREAM)
        _noodles(d, wavy=True)
        _steam(d)
        # shine
        d.arc([8, 6, 24, 18], 200, 80, fill=rgb(CREAM))
    elif kind == "HARD":
        _fill_broth(d, CREAM, INK)
        _noodles(d, wavy=False, pale=True)
        # stiff vertical sticks
        for x in (10, 16, 22, 28, 34):
            d.line([x, 6, x, 34], fill=rgb(NOODLE))
            d.line([x + 1, 6, x + 1, 34], fill=rgb(INK))
    elif kind == "SOGGY":
        _fill_broth(d, BROTH, INK)
        d.ellipse([8, 10, 40, 36], fill=rgb(NOODLE), outline=rgb(INK))
        d.ellipse([14, 16, 28, 28], fill=rgb(INK))
        d.ellipse([24, 20, 36, 32], fill=rgb(CREAM))
    else:
        raise ValueError(kind)
    return img


def make_pot(path):
    sheet = new_rgb((POT_W * 8, POT_H * 8 * len(POT_STATES)), CREAM)
    for i, name in enumerate(POT_STATES):
        sheet.paste(make_pot_state(name), (0, i * POT_H * 8))
    sheet.save(path)
    return path


# ---------------------------------------------------------------- font (cream bg, ink glyphs)
def make_font(path):
    n = len(FONT_CHARS)
    img = new_rgb((n * 8, 8), CREAM)
    for i, ch in enumerate(FONT_CHARS):
        if ch == " ":
            continue
        tw, th = text_size(ch, 1)
        x = i * 8 + (8 - tw) // 2
        y = (8 - th) // 2
        draw_text(img, x, y, ch, rgb(INK), scale=1)
    img.save(path)
    return path


# ---------------------------------------------------------------- gauge (112x8, green perfect, gold good)
def make_gauge(path):
    # 14 tiles. perfect = bar pixels 48-63 (tiles 6-7). good = 32-47 and 64-79 (tiles 4-5, 8-9)
    w = GAUGE_REGION[2] * 8
    img = new_rgb((w, 8), INK)
    d = ImageDraw.Draw(img)
    d.rectangle([0, 1, w - 1, 6], fill=rgb(CREAM), outline=rgb(INK))
    d.rectangle([0, 1, 1, 6], fill=rgb(GOLD))  # left cap
    d.rectangle([w - 2, 1, w - 1, 6], fill=rgb(GOLD))
    d.rectangle([32, 1, 47, 6], fill=rgb(GOLD))
    d.rectangle([64, 1, 79, 6], fill=rgb(GOLD))
    d.rectangle([48, 1, 63, 6], fill=rgb(GREENZ))
    img.save(path)
    return path


# ---------------------------------------------------------------- sprites
def sprite_from_grid(grid, colors, path, frames=1):
    """grid is 8 rows of digits '0'/'.'  or a list of frames."""
    if isinstance(grid[0], str):
        frames_g = [grid]
    else:
        frames_g = grid
    img = Image.new("RGBA", (8 * len(frames_g), 8), (0, 0, 0, 0))
    d = ImageDraw.Draw(img)
    for i, g in enumerate(frames_g):
        for y, row in enumerate(g):
            for x, ch in enumerate(row):
                if ch != ".":
                    d.point((i * 8 + x, y), fill=rgb(colors[int(ch) - 1]))
    img.save(path)
    return path


def make_cursor(path):
    # downward arrow / ladle tip, 8x8
    grid = [
        "..33..",
        ".3333.",
        "333333",
        "..11..",
        "..11..",
        "..11..",
        "..22..",
        "...2..",
    ]
    # pad to 8
    grid = [r.ljust(8, ".")[:8] for r in grid]
    # the strings above are 6-7 long; fix properly:
    grid = [
        "..33....",
        ".3333...",
        "333333..",
        "..11....",
        "..11....",
        "..11....",
        "..22....",
        "...2....",
    ]
    return sprite_from_grid(grid, CURSOR_COLS, path)


def make_egg(path):
    grid = [
        "..111...",
        ".11111..",
        "1111111.",
        "1112211.",
        "1112211.",
        ".11111..",
        "..111...",
        "........",
    ]
    return sprite_from_grid(grid, EGG_COLS, path)


def make_onion(path):
    grid = [
        "...2....",
        "...2.2..",
        "..1212..",
        ".11111..",
        "1111111.",
        ".11.11..",
        "..1.1...",
        "........",
    ]
    return sprite_from_grid(grid, ONION_COLS, path)


def make_blank(path):
    new_rgb((8, 8), CREAM).save(path)
    return path


# ---------------------------------------------------------------- results
def make_results(path):
    img = new_rgb((SCREEN_W, SCREEN_H), CREAM)
    d = ImageDraw.Draw(img)
    ink, cream, red, gold = rgb(INK), rgb(CREAM), rgb(RED), rgb(GOLD)
    # same 4 colours as title so we can reuse TITLE_PAL / results has own
    d.rectangle([0, 0, 159, 23], fill=red)
    d.line([0, 23, 159, 23], fill=ink)
    tw, _ = text_size("RAMEN TIME", 1)
    draw_text(img, (SCREEN_W - tw) // 2 + 1, 9, "RAMEN TIME", ink, scale=1)
    draw_text(img, (SCREEN_W - tw) // 2, 8, "RAMEN TIME", gold, scale=1)

    # frame
    d.rectangle([8, 32, 151, 119], outline=ink)
    d.rectangle([9, 33, 150, 118], outline=gold)

    # small bowl decoration at the bottom of the box
    d.ellipse([56, 84, 103, 116], fill=red, outline=ink)
    d.ellipse([62, 88, 97, 108], fill=gold, outline=ink)
    d.ellipse([66, 90, 93, 106], fill=red)
    for x in range(70, 90, 2):
        d.point((x, 98 + (x // 2) % 2), fill=gold)

    d.rectangle([0, 128, 159, 143], fill=red)
    # "PRESS START" / "SELECT: TITLE" will be printed by C on rows 16-17,
    # but bake a red bar so the font (cream bg) sits on red... font has cream
    # background tiles, that's fine.

    img.save(path)
    return path


def write_layout(path):
    px, py, pw, ph = PRESS_REGION
    ox, oy, ow, oh = POT_REGION
    gx, gy, gw, gh = GAUGE_REGION
    jx, jy, jw, jh = JUDGE_REGION
    rx, ry, rw, rh = PROMPT_REGION
    lines = [
        "/* Auto-generated by assets/make_art.py — do not hand-edit. */",
        "#ifndef LAYOUT_H",
        "#define LAYOUT_H",
        "",
        f"#define PRESS_X {px}",
        f"#define PRESS_Y {py}",
        f"#define PRESS_W {pw}",
        f"#define PRESS_H {ph}",
        "",
        f"#define POT_X {ox}",
        f"#define POT_Y {oy}",
        f"#define POT_W {ow}",
        f"#define POT_H {oh}",
        f"#define POT_MAP_W {ow}",
        f"#define POT_STATE_TILES {ow * oh}",
        f"#define POT_STATE_COUNT {len(POT_STATES)}",
        "",
        f"#define GAUGE_X {gx}",
        f"#define GAUGE_Y {gy}",
        f"#define GAUGE_W {gw}",
        f"#define GAUGE_H {gh}",
        f"#define GAUGE_PX {gw * 8}",
        "#define ZONE_PERFECT_LO 48",
        "#define ZONE_PERFECT_HI 63",
        "#define ZONE_GOOD_LO 32",
        "#define ZONE_GOOD_HI 79",
        "",
        f"#define PROMPT_X {rx}",
        f"#define PROMPT_Y {ry}",
        f"#define PROMPT_W {rw}",
        f"#define JUDGE_X {jx}",
        f"#define JUDGE_Y {jy}",
        f"#define JUDGE_W {jw}",
        "",
        f"#define HUD_ROW {HUD_ROW}",
        "#define HINT_ROW 16",
        "",
        "#define FONT_SPACE 0",
        "#define FONT_DIGIT 1",
        "#define FONT_ALPHA 11",
        "#define FONT_BANG 37",
        "#define FONT_SLASH 38",
        "#define FONT_COLON 39",
        "#define FONT_DASH 40",
        f"#define FONT_CHAR_COUNT {len(FONT_CHARS)}",
        "",
        "/* pot state indices, matching the sheet rows in assets/pot.png */",
        "#define POT_ST_WATER    0",
        "#define POT_ST_BUBBLE0  1",
        "#define POT_ST_BUBBLE1  2",
        "#define POT_ST_NOODLES  3",
        "#define POT_ST_SOUP     4",
        "#define POT_ST_PERFECT  5",
        "#define POT_ST_HARD     6",
        "#define POT_ST_SOGGY    7",
        "",
        "#endif /* LAYOUT_H */",
        "",
    ]
    with open(path, "w", encoding="utf-8") as fh:
        fh.write("\n".join(lines))
    return path


CONVERSIONS = [
    # png, name, mode, extra
    ("title.png", "title", "bg", ["--map", "--dedup", "--palette", TITLE_PAL]),
    ("play.png", "play", "bg", ["--map", "--dedup", "--palette", PLAY_PAL]),
    ("pot.png", "pot", "bg", ["--map", "--dedup", "--palette", POT_PAL]),
    ("results.png", "results", "bg", ["--map", "--dedup", "--palette", TITLE_PAL]),
    ("font.png", "font", "bg", ["--palette", FONT_PAL]),
    ("gauge.png", "gauge", "bg", ["--map", "--dedup", "--palette", GAUGE_PAL]),
    ("blank.png", "blank", "bg", ["--palette", FONT_PAL]),
    ("cursor.png", "cursor", "sprite", []),
    ("egg.png", "egg", "sprite", []),
    ("onion.png", "onion", "sprite", []),
]


def convert(png_name, name, mode, extra):
    cmd = [
        sys.executable, PNG2GBC, os.path.join(HERE, png_name),
        "--mode", mode, "--name", name, "-o", os.path.join(GEN, name),
        *extra,
    ]
    subprocess.run(cmd, check=True)


def make_sheet():
    title = Image.open(os.path.join(HERE, "title.png")).convert("RGB")
    play = Image.open(os.path.join(HERE, "play.png")).convert("RGB")
    pot = Image.open(os.path.join(HERE, "pot.png")).convert("RGB")
    results = Image.open(os.path.join(HERE, "results.png")).convert("RGB")
    font = Image.open(os.path.join(HERE, "font.png")).convert("RGB")
    gauge = Image.open(os.path.join(HERE, "gauge.png")).convert("RGB")
    W = 160 * 2 + 16
    H = 144 * 2 + pot.height + 40
    sheet = Image.new("RGB", (W, H), (30, 30, 36))
    sheet.paste(title, (0, 0))
    sheet.paste(play, (168, 0))
    sheet.paste(results, (0, 152))
    sheet.paste(pot, (168, 152))
    sheet.paste(font, (168, 152 + pot.height + 8))
    sheet.paste(gauge.resize((gauge.width * 2, gauge.height * 2), Image.NEAREST),
                (168, 152 + pot.height + 24))
    x = 0
    for name in ("cursor.png", "egg.png", "onion.png"):
        im = Image.open(os.path.join(HERE, name)).convert("RGBA")
        plate = Image.new("RGB", (im.width * 4, im.height * 4), (40, 40, 48))
        im4 = im.resize((im.width * 4, im.height * 4), Image.NEAREST)
        plate.paste(im4, (0, 0), im4)
        sheet.paste(plate, (x, 152 + 144 + 8))
        x += plate.width + 8
    out = os.path.join(HERE, "sheet.png")
    sheet.save(out)
    print(f"wrote {out}")


def main(argv=None):
    ap = argparse.ArgumentParser()
    ap.add_argument("--png-only", action="store_true")
    args = ap.parse_args(argv)

    os.makedirs(GEN, exist_ok=True)
    jobs = [
        ("title.png", make_title),
        ("play.png", make_play),
        ("pot.png", make_pot),
        ("results.png", make_results),
        ("font.png", make_font),
        ("gauge.png", make_gauge),
        ("blank.png", make_blank),
        ("cursor.png", make_cursor),
        ("egg.png", make_egg),
        ("onion.png", make_onion),
    ]
    for name, fn in jobs:
        fn(os.path.join(HERE, name))
        print(f"drew {name}")
    print(f"wrote {write_layout(os.path.join(GEN, 'layout.h'))}")

    if args.png_only:
        make_sheet()
        return 0

    for item in CONVERSIONS:
        convert(*item)
        print(f"converted {item[0]}")
    make_sheet()
    return 0


if __name__ == "__main__":
    raise SystemExit(main())
