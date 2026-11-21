# RAMEN TIME — measured playtest timing

PyBoy `--boot 90`. Title is already up.

After `press:start:4` the playfield starts on the following frames:

- First play vsync / intro start: frame 104 (script time: immediately after `press:start:4`, which lands at frame 103)
- Intro lasts 20 frames. First moving-cursor frame: 123 (`wait:20` after start)
- Round 1 cursor is 1 pixel every 2 frames
- PERFECT window (cursor_x 48–63): frames 219–250 relative to boot
- Mid-window (safe scripted hit): frame 227 = `wait:104` after the play snap at 123

Official script is in `logs/playtest-script.txt`.

The first official boil hit is PERFECT (+100). Remaining phases in the scripted run are left to time out (MISS / SKIP / late STOP = SOGGY) so the results screen is reached.
