Implement RAMEN TIME NOW. Do not research the web. Do not search GBDK sources. Everything you need is already in this repo.

Read only if you haven't: GAME.md, SKILL.md (cookbook sections), tools/png2gbc.py --help, tools/artfont.py (library use), and maybe src/main.c.tilehop-reference.c for C patterns (sfx, title blink, palettes, vsync). Then write files.

## Do this, in order, without extra exploration

1. Write `logs/progress.md` saying you started implementing.
2. Write `assets/make_art.py` that draws all PNGs with Pillow + `from artfont import draw_text` and converts them with `tools/png2gbc.py`. TOOLS path is `os.path.join(ROOT, "tools")` (this repo is flat). Palettes are bright and warm (cream, red, gold, green onion, egg yolk, dark brown). Assets:
   - title 160x144 map, dedup: big ramen bowl + steam + "RAMEN TIME" + tile-aligned "PRESS START"
   - play background 160x144 map, dedup: kitchen + counter + pot frame. Pot interior is a tile-aligned rectangle C can swap.
   - pot interior tileset (water, bubbles x2, hard noodles, perfect noodles, soggy noodles, soup-tinted broth, egg, green onion)
   - hud + digits 0-9
   - gauge tiles (bar, green zone, caps)
   - banners: PERFECT, GOOD, MISS, EGG, ONION, SKIP, and maybe HARD/SOGGY
   - results 160x144 map, dedup: rating area, "PRESS START", "SELECT: TITLE"
   - cursor sprite (arrow or ladle), 8x8, mode sprite
   - blank tile
   Run it with `.venv/bin/python3 assets/make_art.py`. Open the preview PNGs. Fix art if text is clipped or the pot is unreadable.
3. Write `src/main.c` for the full game in GAME.md. Follow GAME.md timing EXACTLY:
   - 3 rounds, 5 phases: BOIL, NOODLES, SOUP, TOPPING (A egg / B onion / no press SKIP), STOP
   - bar 112 px; PERFECT 48-63; GOOD 32-47 and 64-79
   - INTRO_FRAMES=20, then cursor: r1 +1px/2f, r2 +1px/1f, r3 +2px/1f
   - JUDGE_FRAMES=45
   - scores 100/60/0, topping +20, ratings S>=1100 A>=900 B>=700 C>=500 else D
   - STOP sets doneness: perfect/good/early miss=HARD/late miss=SOGGY, show in pot and on results
   - START starts / retries; SELECT returns to title
   - edge-triggered A/B; waitpadup after title START
   - vsync() first in the loop; CGB palettes guarded; VBK_REG reset to VBK_TILES
   - tile slot #defines + #error if BG>256 or total>384
   - simple channel-1 SFX (high perfect, mid good, low/noise miss)
   - `#pragma disable_warning 158` and `110`
   ROM title is already RAMENTIME in build.sh. Output is build/ramen-time.gbc. Use `-Wm-yc`.
4. `source ./env.sh && ./build.sh --assets` (or compile after art). Fix compiler errors.
5. Measure timing with playtest. Write `logs/timing.md` and a one-line `logs/playtest-script.txt` that snaps: title, gameplay (pot+gauge+BOIL), a visible PERFECT on the first boil, and the results screen (script later phases; they can be GOOD/MISS, but first boil must be PERFECT). Official ROM is 3 rounds. You may also compile `-DROUND_TOTAL=1` only as a helper, but ship the 3-round ROM and a strip from that ROM that reaches results.
6. Run:
   `.venv/bin/python3 tools/playtest.py --rom build/ramen-time.gbc --script "$(cat logs/playtest-script.txt)" --out build/playtest-strip.png --frames-dir build/frames --strict --json`
   Inspect the strip and frames. Fix until `--strict` is clean and the four shots look right (legible text, colourful, PERFECT visible, results rating visible).
7. Copy evidence to `docs/images/`. Write a short README.md (controls, rules, build). Confirm header byte 0x143 is 0x80.

Environment: GBDK is $HOME/gbdk/, python is .venv/bin/python3. No sudo, no other projects, no /Volumes, no SD card copy.

Start writing make_art.py immediately after the short reads. Do not stall on research.
