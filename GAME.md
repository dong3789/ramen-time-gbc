# RAMEN TIME — design + acceptance

A cute Game Boy Color ramen-cooking timing game. English text only (GB font).
Bright, warm, Korean-convenience-store-ramen energy: red pot, gold broth, green onion, soft egg.

## One-paragraph game

Title screen (bowl + steam + "RAMEN TIME" + blinking PRESS START). START begins a run of **3 rounds**. Each round is five timing steps over one pot of ramen: **BOIL** water, drop **NOODLES**, add **SOUP** powder, optional **TOPPING** (A = egg, B = green onion), then **STOP** the heat. A cursor races across a gauge; press in the green zone for PERFECT / near it for GOOD / else MISS. The pot itself changes: bubbles, noodles appearing, powder tinting the broth, toppings landing, and the final noodle state (HARD / PERFECT / SOGGY) comes from the STOP step (early miss = hard, late miss = soggy, perfect/good = those looks). Score accumulates. After three rounds a results screen shows score + letter rating. START retries, SELECT returns to the title from play or results.

## Controls

| Button | Action |
| --- | --- |
| START | Title → play. Results → play again. |
| SELECT | Play or results → title. |
| A | Hit the timing window on BOIL / NOODLES / SOUP / STOP. On TOPPING, A adds egg if in zone. |
| B | Only on TOPPING: add green onion if in zone. |

Edge-trigger A/B (press this frame, not held). If the cursor reaches the end with no press, that is a late MISS (STOP: SOGGY). TOPPING with no press is SKIP (0 bonus, not a miss).

## Timing (must be deterministic for playtest)

- Gauge bar is **112 pixels** wide (14 tiles), drawn on a fixed row near the bottom.
- PERFECT zone: bar pixels **48–63** (16 px, marked in green/bright).
- GOOD zone: bar pixels **32–47** and **64–79**.
- Everywhere else, or no press: MISS.
- After a phase is entered, wait **INTRO_FRAMES = 20** with the prompt shown and the cursor parked at 0.
- Then the cursor moves:
  - Round 1: +1 pixel every 2 frames  (cursor_x = phase_frames / 2)
  - Round 2: +1 pixel every 1 frame   (cursor_x = phase_frames)
  - Round 3: +2 pixels every 1 frame  (cursor_x = phase_frames * 2)
- After a hit (or auto-miss at the end), freeze **JUDGE_FRAMES = 45** showing PERFECT / GOOD / MISS / EGG / ONION / SKIP, update the pot, then go to the next phase (or next round / results).
- `vsync()` at the top of the main loop. Joypad is edge-triggered from the previous frame.

Round 1 PERFECT press: first frame where `cursor_x` is in 48–63, i.e. after intro, when `phase_frames == 96` (because 96/2 = 48). Write the measured playtest script to `logs/playtest-script.txt` (one line) and `logs/timing.md` after you verify it in PyBoy.

Compile-time knobs (for a short results-path ROM if needed, but the shipped ROM is 3 rounds):

```
#ifndef ROUND_TOTAL
#define ROUND_TOTAL 3
#endif
```

## Scoring + rating

- PERFECT = 100, GOOD = 60, MISS = 0
- TOPPING in zone: +20 (egg or onion). SKIP = 0. Out-of-zone press on topping = 0 and show MISS.
- Max = 3 * (100*4 + 20) = 1260
- Ratings: S ≥ 1100, A ≥ 900, B ≥ 700, C ≥ 500, else D
- HUD during play: `R 1/3` and `SCORE 0000` (4 digits is enough)
- STOP step also sets doneness: PERFECT → perfect wavy noodles, GOOD → decent noodles, early MISS → HARD stiff sticks, late MISS → SOGGY mush. Show this in the pot immediately, and again on the results screen.

## Screens / art (tile-economical, 4 colours per tile)

Bright palettes, cute pixel art, no Korean glyphs (English only).

1. **title** 160×144 background, `--map --dedup`. Big ramen bowl, steam curls, chopsticks, "RAMEN TIME" (artfont, scale 2 or 3), blinking "PRESS START" on a tile-aligned band. Warm cream / red / gold / dark brown.
2. **play background** kitchen: wallpaper, wood counter, a large pot in the center (reusable tiles, `--map --dedup`). Leave a tile-aligned region for the pot interior so C can swap water / bubbles / noodles / toppings at runtime.
3. **pot interior tiles** (or a small tileset sheet): empty water, bubbling water (2 frames to animate), dry/hard noodles, perfect wavy noodles + steam, soggy mush, broth tint after soup, egg half, green-onion curls. These must be tile-aligned and swappable with `set_bkg_tiles`.
4. **hud** + **digits** 0–9 (and maybe `/`).
5. **gauge** tiles: empty bar, filled trail optional, green zone, end caps. Cursor is a **sprite** (arrow or ladle).
6. **banners** for PERFECT / GOOD / MISS / EGG / ONION / SKIP / HARD / SOGGY, tile-aligned so they can be stamped on and erased.
7. **results** 160×144 or a composed box: rating letter, score, final bowl, "PRESS START" / "SELECT: TITLE".
8. **blank** tile for erasing.

Keep a `#define` tile-slot block in `main.c` and `#error` if BG > 256 or total > 384. Use `--dedup` on backgrounds. Do not hand-edit `src/gen/*`.

Sprites: cursor plus maybe 1–2 steam puffs. Stay under 10 per scanline. `move_sprite(n, x+8, y+16)`.

## Sound

Simple channel-1 (and noise for miss) SFX: start, perfect (high), good (mid), miss (low/noise), topping blip, results sting. Init NR52/NR51/NR50 as in the starter.

## Acceptance tests

Write `logs/playtest-script.txt` so this is true, then run with `--strict` and **look at the PNGs**:

1. **Title** — boot / title snap: "RAMEN TIME" and "PRESS START" are legible, bowl is visible, colours are warm and bright, not a blank or 2-colour screen.
2. **Gameplay** — after START, pot + gauge + phase prompt (BOIL) are visible, cursor is on the bar.
3. **PERFECT hit** — script presses A in the first BOIL perfect window; a snap shows the word PERFECT (or a clear PERFECT banner) and the pot has gained bubbles.
4. **Results** — after three rounds (script may hit later phases loosely, but at least the first hit is PERFECT), results screen shows a letter rating and the score. START/SELECT prompts visible.

Also: `./build.sh` has no compiler warnings, ROM header byte `0x143 == 0x80`, size under 512 KiB, playtest `--strict` exits 0 with **no warnings**.

Copy evidence into `docs/images/` (strip + individual title / play / perfect / results frames).

## Layout of this repo

```
assets/make_art.py     draw PNGs + call tools/png2gbc.py (TOOLS = ../tools)
src/main.c             the game (start from the comments in SKILL.md / tile-hop reference)
src/gen/*              generated only
build.sh / Makefile    already written; output build/ramen-time.gbc
tools/                 kit tools (do not rewrite unless you find a bug)
SKILL.md AGENTS.md     follow these
```

Python: always use `.venv/bin/python3` (already has pillow, numpy, pyboy). GBDK is at `$HOME/gbdk/` (trailing slash). `source ./env.sh` if needed.
