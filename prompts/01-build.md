You are building RAMEN TIME, a complete Game Boy Color game in this repo.

Read first, in this order, and follow them:
1. ./AGENTS.md
2. ./SKILL.md
3. ./GAME.md   ← the design, timing numbers, scoring, and acceptance tests. Do not silently change the timing model; playtests depend on it.

Your job is to finish the kit loop until the acceptance tests in GAME.md pass:

    plan (already in GAME.md) → PNG art via assets/make_art.py + tools/png2gbc.py
    → C in src/main.c (one file, start from the GBDK cookbook in SKILL.md; you may peek at src/main.c.tilehop-reference.c but do NOT ship tile-hop)
    → ./build.sh → tools/playtest.py strips → LOOK AT THE FRAME PNGs → fix → repeat

A ROM that compiles is not done. Never stop on a blank screen, illegible text, or a playtest warning.

## Environment (already installed)

```bash
source ./env.sh
# GBDKDIR=$HOME/gbdk/   (trailing slash)
# python is .venv/bin/python3  (pillow, numpy, pyboy)
./tools/check_env.sh
```

Use `.venv/bin/python3` for every python command. Do not apt/brew/sudo. Do not touch any other project, service, tunnel, or `/Volumes/BOOT`, and do not copy to the SD card yourself (the supervisor will).

`build.sh` and `Makefile` already exist and write `build/ramen-time.gbc`. You may edit them if you need extra compiler flags, but keep the output name and `-Wm-yc`.

## Art rules

- Write `assets/make_art.py` that draws every PNG with Pillow + `tools/artfont.py` (`draw_text`) and then invokes `tools/png2gbc.py`. Set `TOOLS` to the repo's `tools/` directory (this project is flat: `tools/png2gbc.py`, not `../../tools`).
- 4 colours per background tile, 3+transparent per sprite. Align runtime-swapped regions to 8×8 tiles.
- `--dedup` on full-screen backgrounds. `--map` for title, play bg, results if they are maps.
- `--palette` when assets must share colours. Prefer hand palettes over `--quantize`.
- Look at every `*_preview.png` and the source PNGs before writing C. If text is clipped or a bowl is a muddy blob, fix the art first.
- Do not hand-edit `src/gen/*`.

## C rules

- One `src/main.c`. Tile slot `#define`s + `#error` if BG > 256 or total > 384.
- `vsync()` first in the main loop. Edge-triggered joypad. `waitpadup()` after START on the title.
- `DEVICE_SUPPORTS_COLOR` around CGB palette / VBK writes. Always set `VBK_REG` back to `VBK_TILES`.
- Sprites: `move_sprite(n, x + 8, y + 16)`. Park unused sprites at (0,0).
- Follow GAME.md timing exactly so a scripted A press can hit PERFECT.
- After implementing, measure frames in PyBoy and write:
  - `logs/timing.md` (when the first PERFECT window opens, in frames after boot and after START)
  - `logs/playtest-script.txt` (single line, no spaces that break parsing) that captures: title, gameplay, a visible PERFECT, and a results screen.
- Simple SFX as specified.
- `#pragma disable_warning 158` and `110` if SDCC complains about tile index math, same as the starter.

## Playtest + looking

Default boot is 90 frames; the title must already be up.

```bash
source ./env.sh
./build.sh --assets
.venv/bin/python3 tools/playtest.py --rom build/ramen-time.gbc \
  --script "$(cat logs/playtest-script.txt)" \
  --out build/playtest-strip.png --frames-dir build/frames --strict --json
```

Then **open and visually inspect** `build/playtest-strip.png` and the frames in `build/frames/`. Check, in order: display on, palette warm and colourful, text legible, input changed the screen, PERFECT banner visible, pot actually looks like a pot of ramen, results rating visible.

If anything is wrong, fix art or C and repeat. Iterate until `--strict` is clean and the four required shots look right.

If three full rounds make the script painfully long, you may also build a `-DROUND_TOTAL=1` ROM *only* to capture a results frame faster, but the shipped `build/ramen-time.gbc` must be the 3-round game, and the official strip should still reach results on that 3-round ROM (script all phases; later phases can be GOOD/MISS, but the first BOIL must be a visible PERFECT).

## Deliverables before you stop

- [ ] `assets/make_art.py` + source PNGs
- [ ] `src/main.c` implementing the full game in GAME.md
- [ ] `./build.sh` succeeds with no warnings, `build/ramen-time.gbc`, header 0x143 is 0x80
- [ ] `logs/timing.md` and `logs/playtest-script.txt`
- [ ] `build/playtest-strip.png` + `build/frames/*.png` showing title, play, PERFECT, results
- [ ] those same images copied to `docs/images/`
- [ ] `README.md` (short: controls, rules, how to build, links to the images)
- [ ] playtest `--strict` exit 0, no warnings

Write a short status to `logs/progress.md` as you go so work can be resumed if you are interrupted.

Start now. Do the art first, look at it, then the C, then playtest and fix until it is actually done.
