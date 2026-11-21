/*
 * RAMEN TIME — a Game Boy Color cooking timing game (GBDK-2020).
 *
 * Title -> 3 rounds of 5 timing steps -> results.
 * Timing (see GAME.md / layout.h): 112 px gauge, PERFECT 48-63, GOOD 32-79
 * (but only 32-47 and 64-79 are GOOD; 48-63 is PERFECT), intro 20 frames,
 * cursor speeds 1px/2f, 1px/1f, 2px/1f, judge 45 frames.
 */
#include <gbdk/platform.h>
#include <gb/cgb.h>
#include <gb/gb.h>
#include <stdint.h>
#include <string.h>

#include "gen/title.h"
#include "gen/play.h"
#include "gen/pot.h"
#include "gen/results.h"
#include "gen/font.h"
#include "gen/gauge.h"
#include "gen/blank.h"
#include "gen/cursor.h"
#include "gen/egg.h"
#include "gen/onion.h"
#include "gen/layout.h"

#pragma disable_warning 158
#pragma disable_warning 110

/* ------------------------------------------------------------------ tiles */
/* Play-screen VRAM map. Title and results reload their own layouts. */
#define T_PLAY   0
#define T_POT    (T_PLAY + PLAY_TILE_COUNT)
#define T_FONT   (T_POT + POT_TILE_COUNT)
#define T_GAUGE  (T_FONT + FONT_TILE_COUNT)
#define T_BLANK  (T_GAUGE + GAUGE_TILE_COUNT)
#define T_SPR    (T_BLANK + BLANK_TILE_COUNT)
#define T_CURSOR T_SPR
#define T_EGG    (T_CURSOR + CURSOR_TILE_COUNT)
#define T_ONION  (T_EGG + EGG_TILE_COUNT)
#define BG_TILES T_SPR
#define ALL_TILES (T_ONION + ONION_TILE_COUNT)

#if BG_TILES > 256
#error "background tiles must fit in a 0..255 map index"
#endif
#if ALL_TILES > 384
#error "too many tiles for the 384-tile VRAM"
#endif

/* Results-screen font base (results tiles occupy 0..RESULTS_TILE_COUNT-1) */
#define T_RFONT  RESULTS_TILE_COUNT
#define T_RBLANK (T_RFONT + FONT_TILE_COUNT)

/* Palettes */
#define PAL_PLAY   0
#define PAL_POT    1
#define PAL_FONT   2
#define PAL_GAUGE  3
#define PAL_TITLE  0
#define PAL_RES    0

#define SPAL_CURSOR 0
#define SPAL_EGG    1
#define SPAL_ONION  2

#define SPR_CURSOR 0
#define SPR_EGG    1
#define SPR_ONION  2

#ifndef ROUND_TOTAL
#define ROUND_TOTAL 3
#endif

#define INTRO_FRAMES 20
#define JUDGE_FRAMES 45

#define PHASE_BOIL    0
#define PHASE_NOODLES 1
#define PHASE_SOUP    2
#define PHASE_TOPPING 3
#define PHASE_STOP    4
#define PHASE_COUNT   5

#define JUDGE_MISS    0
#define JUDGE_GOOD    1
#define JUDGE_PERFECT 2
#define JUDGE_EGG     3
#define JUDGE_ONION   4
#define JUDGE_SKIP    5

#define DONE_RAW  0
#define DONE_HARD 1
#define DONE_OK   2
#define DONE_PERF 3
#define DONE_SOGGY 4

/* ------------------------------------------------------------------ state */
static uint16_t score;
static uint8_t round_no;
static uint8_t phase;
static uint8_t pot_state;
static uint8_t doneness;     /* last STOP result */
static uint8_t has_egg;
static uint8_t has_onion;
static uint8_t cursor_x;     /* 0..GAUGE_PX */
static uint8_t prev_joy;
static uint8_t want_title;   /* SELECT was pressed */
static uint16_t frame;

/* ------------------------------------------------------------------ helpers */
static void put_attr_rect(uint8_t x, uint8_t y, uint8_t w, uint8_t h, uint8_t pal) {
    uint8_t row[20];
    uint8_t i;
    if (!DEVICE_SUPPORTS_COLOR) return;
    for (i = 0; i < w; i++) row[i] = pal;
    VBK_REG = VBK_ATTRIBUTES;
    for (i = 0; i < h; i++) set_bkg_tiles(x, y + i, w, 1, row);
    VBK_REG = VBK_TILES;
}

static void hide_all_sprites(void) {
    uint8_t i;
    for (i = 0; i < 40; i++) move_sprite(i, 0, 0);
    HIDE_SPRITES;
}

static uint8_t ch_tile(char c) {
    if (c == ' ') return FONT_SPACE;
    if (c >= '0' && c <= '9') return (uint8_t)(FONT_DIGIT + (c - '0'));
    if (c >= 'A' && c <= 'Z') return (uint8_t)(FONT_ALPHA + (c - 'A'));
    if (c >= 'a' && c <= 'z') return (uint8_t)(FONT_ALPHA + (c - 'a'));
    if (c == '!') return FONT_BANG;
    if (c == '/') return FONT_SLASH;
    if (c == ':') return FONT_COLON;
    if (c == '-') return FONT_DASH;
    return FONT_SPACE;
}

static void print_at_base(uint8_t x, uint8_t y, const char *s, uint8_t base) {
    while (*s) {
        uint8_t t = base + ch_tile(*s++);
        set_bkg_tiles(x++, y, 1, 1, &t);
    }
}

static void print_at(uint8_t x, uint8_t y, const char *s) {
    print_at_base(x, y, s, T_FONT);
}

static void clear_row(uint8_t x, uint8_t y, uint8_t w) {
    uint8_t i, t = T_BLANK;
    for (i = 0; i < w; i++) set_bkg_tiles(x + i, y, 1, 1, &t);
}

static void print_num(uint8_t x, uint8_t y, uint16_t n, uint8_t digits, uint8_t base) {
    uint8_t buf[5];
    uint8_t i;
    for (i = digits; i > 0; i--) {
        buf[i - 1] = base + FONT_DIGIT + (uint8_t)(n % 10);
        n /= 10;
    }
    set_bkg_tiles(x, y, digits, 1, buf);
}

/* ------------------------------------------------------------------ sound */
static void sfx_init(void) {
    NR52_REG = 0x80;
    NR51_REG = 0xFF;
    NR50_REG = 0x77;
}

static void sfx_tone(uint8_t freq_lo, uint8_t freq_hi, uint8_t env) {
    NR10_REG = 0x00;
    NR11_REG = 0x80;
    NR12_REG = env;
    NR13_REG = freq_lo;
    NR14_REG = (uint8_t)(0x80 | (freq_hi & 0x07));
}

static void sfx_noise(uint8_t env, uint8_t poly) {
    NR41_REG = 0x00;
    NR42_REG = env;
    NR43_REG = poly;
    NR44_REG = 0x80;
}

static void sfx_perfect(void) { sfx_tone(0x80, 0x07, 0xF3); }
static void sfx_good(void)    { sfx_tone(0x00, 0x06, 0xF5); }
static void sfx_miss(void)    { sfx_noise(0xA3, 0x47); }
static void sfx_blip(void)    { sfx_tone(0xC0, 0x06, 0xC2); }
static void sfx_start(void)   { sfx_tone(0x40, 0x86, 0xF7); }
static void sfx_result(void)  { sfx_tone(0x00, 0x87, 0xF5); }

/* ------------------------------------------------------------------ palettes */
static void load_play_palettes(void) {
    if (!DEVICE_SUPPORTS_COLOR) return;
    set_bkg_palette(PAL_PLAY, 1, play_palette);
    set_bkg_palette(PAL_POT, 1, pot_palette);
    set_bkg_palette(PAL_FONT, 1, font_palette);
    set_bkg_palette(PAL_GAUGE, 1, gauge_palette);
    set_sprite_palette(SPAL_CURSOR, 1, cursor_palette);
    set_sprite_palette(SPAL_EGG, 1, egg_palette);
    set_sprite_palette(SPAL_ONION, 1, onion_palette);
}

static void load_title_palettes(void) {
    if (!DEVICE_SUPPORTS_COLOR) return;
    set_bkg_palette(PAL_TITLE, 1, title_palette);
}

static void load_results_palettes(void) {
    if (!DEVICE_SUPPORTS_COLOR) return;
    set_bkg_palette(PAL_RES, 1, results_palette);
    set_bkg_palette(PAL_FONT, 1, font_palette);
}

/* ------------------------------------------------------------------ pot / hud */
static void draw_pot(uint8_t state) {
    if (state >= POT_STATE_COUNT) state = POT_ST_WATER;
    pot_state = state;
    set_bkg_based_tiles(POT_X, POT_Y, POT_W, POT_H,
                        pot_map + (uint16_t)state * POT_STATE_TILES, T_POT);
    put_attr_rect(POT_X, POT_Y, POT_W, POT_H, PAL_POT);
}

static void draw_topping_sprites(void) {
    if (has_egg) {
        set_sprite_tile(SPR_EGG, T_EGG);
        set_sprite_prop(SPR_EGG, SPAL_EGG);
        /* left side of the pot interior */
        move_sprite(SPR_EGG, (uint8_t)(POT_X * 8 + 8 + 8), (uint8_t)(POT_Y * 8 + 16 + 16));
    } else {
        move_sprite(SPR_EGG, 0, 0);
    }
    if (has_onion) {
        set_sprite_tile(SPR_ONION, T_ONION);
        set_sprite_prop(SPR_ONION, SPAL_ONION);
        move_sprite(SPR_ONION, (uint8_t)(POT_X * 8 + 24 + 8), (uint8_t)(POT_Y * 8 + 16 + 16));
    } else {
        move_sprite(SPR_ONION, 0, 0);
    }
}

static void draw_cursor(void) {
    uint8_t sx = (uint8_t)(GAUGE_X * 8 + cursor_x);
    uint8_t sy = (uint8_t)(GAUGE_Y * 8 - 8); /* sit just above the bar */
    set_sprite_tile(SPR_CURSOR, T_CURSOR);
    set_sprite_prop(SPR_CURSOR, SPAL_CURSOR);
    move_sprite(SPR_CURSOR, (uint8_t)(sx + 8 - 3), (uint8_t)(sy + 16));
}

static void draw_hud(void) {
    print_at(1, HUD_ROW, "R");
    print_num(3, HUD_ROW, round_no, 1, T_FONT);
    print_at(4, HUD_ROW, "/");
    print_num(5, HUD_ROW, ROUND_TOTAL, 1, T_FONT);
    print_at(8, HUD_ROW, "SCORE");
    print_num(14, HUD_ROW, score, 4, T_FONT);
    put_attr_rect(0, HUD_ROW, 20, 1, PAL_FONT);
}

static const char *phase_prompt(uint8_t p) {
    switch (p) {
        case PHASE_BOIL:    return "    BOIL!     ";
        case PHASE_NOODLES: return "   NOODLES!   ";
        case PHASE_SOUP:    return "    SOUP!     ";
        case PHASE_TOPPING: return "  TOPPING!    ";
        default:            return "    STOP!     ";
    }
}

static const char *judge_text(uint8_t j) {
    switch (j) {
        case JUDGE_PERFECT: return "   PERFECT    ";
        case JUDGE_GOOD:    return "    GOOD      ";
        case JUDGE_EGG:     return "     EGG      ";
        case JUDGE_ONION:   return "    ONION     ";
        case JUDGE_SKIP:    return "    SKIP      ";
        default:            return "    MISS      ";
    }
}

/* ------------------------------------------------------------------ playfield */
static void load_play_tiles(void) {
    set_bkg_data(T_PLAY, PLAY_TILE_COUNT, play_tiles);
    set_bkg_data(T_POT, POT_TILE_COUNT, pot_tiles);
    set_bkg_data(T_FONT, FONT_TILE_COUNT, font_tiles);
    set_bkg_data(T_GAUGE, GAUGE_TILE_COUNT, gauge_tiles);
    set_bkg_data(T_BLANK, BLANK_TILE_COUNT, blank_tiles);
    set_sprite_data(T_CURSOR, CURSOR_TILE_COUNT, cursor_tiles);
    set_sprite_data(T_EGG, EGG_TILE_COUNT, egg_tiles);
    set_sprite_data(T_ONION, ONION_TILE_COUNT, onion_tiles);
}

static void draw_playfield(void) {
    set_bkg_based_tiles(0, 0, 20, 18, play_map, T_PLAY);
    put_attr_rect(0, 0, 20, 18, PAL_PLAY);
    put_attr_rect(0, HUD_ROW, 20, 1, PAL_FONT);
    put_attr_rect(PROMPT_X, PROMPT_Y, 18, 1, PAL_FONT);
    put_attr_rect(0, JUDGE_Y, 20, 1, PAL_FONT);
    put_attr_rect(0, HINT_ROW, 20, 1, PAL_FONT);
    set_bkg_based_tiles(GAUGE_X, GAUGE_Y, GAUGE_W, GAUGE_H, gauge_map, T_GAUGE);
    put_attr_rect(GAUGE_X, GAUGE_Y, GAUGE_W, GAUGE_H, PAL_GAUGE);
    draw_hud();
    print_at(1, HINT_ROW, "A: HIT   B: TOPPING");
    draw_pot(pot_state);
    SHOW_SPRITES;
    draw_topping_sprites();
}

/* ------------------------------------------------------------------ phases */
static uint8_t cursor_pos(uint8_t r, uint16_t pf) {
    if (r <= 1) return (uint8_t)(pf / 2);          /* 1 px every 2 frames */
    if (r == 2) return (uint8_t)pf;                 /* 1 px / frame */
    /* round 3+: 2 px / frame, cap at 255 */
    if (pf > 127) return 255;
    return (uint8_t)(pf * 2);
}

static uint8_t zone_of(uint8_t x) {
    if (x >= ZONE_PERFECT_LO && x <= ZONE_PERFECT_HI) return JUDGE_PERFECT;
    if (x >= ZONE_GOOD_LO && x <= ZONE_GOOD_HI) return JUDGE_GOOD;
    return JUDGE_MISS;
}

static void apply_phase_visual(uint8_t p, uint8_t judge, uint8_t late) {
    switch (p) {
        case PHASE_BOIL:
            if (judge == JUDGE_MISS) draw_pot(POT_ST_WATER);
            else draw_pot(POT_ST_BUBBLE0);
            break;
        case PHASE_NOODLES:
            if (judge == JUDGE_MISS) {
                /* no noodles in the pot */
            } else {
                draw_pot(POT_ST_NOODLES);
            }
            break;
        case PHASE_SOUP:
            if (judge != JUDGE_MISS) draw_pot(POT_ST_SOUP);
            break;
        case PHASE_TOPPING:
            if (judge == JUDGE_EGG) has_egg = 1;
            if (judge == JUDGE_ONION) has_onion = 1;
            draw_topping_sprites();
            break;
        case PHASE_STOP:
            if (judge == JUDGE_PERFECT) {
                doneness = DONE_PERF;
                draw_pot(POT_ST_PERFECT);
            } else if (judge == JUDGE_GOOD) {
                doneness = DONE_OK;
                draw_pot(POT_ST_SOUP);
            } else if (late) {
                doneness = DONE_SOGGY;
                draw_pot(POT_ST_SOGGY);
            } else {
                doneness = DONE_HARD;
                draw_pot(POT_ST_HARD);
            }
            break;
    }
}

static uint8_t judge_points(uint8_t j) {
    if (j == JUDGE_PERFECT) return 100;
    if (j == JUDGE_GOOD) return 60;
    if (j == JUDGE_EGG || j == JUDGE_ONION) return 20;
    return 0;
}

static void play_judge_sfx(uint8_t j) {
    if (j == JUDGE_PERFECT) sfx_perfect();
    else if (j == JUDGE_GOOD) sfx_good();
    else if (j == JUDGE_EGG || j == JUDGE_ONION) sfx_blip();
    else if (j == JUDGE_SKIP) sfx_blip();
    else sfx_miss();
}

/* Returns 1 if SELECT requested a return to title. */
static uint8_t run_phase(uint8_t p) {
    uint8_t intro = INTRO_FRAMES;
    uint16_t pf = 0;
    uint8_t judge = JUDGE_MISS;
    uint8_t late = 0;
    uint8_t done = 0;
    uint8_t wait;
    uint8_t joy, pressed;

    phase = p;
    cursor_x = 0;
    if (p == PHASE_BOIL) {
        draw_pot(POT_ST_BUBBLE0);
    }
    clear_row(0, PROMPT_Y, 20);
    clear_row(0, JUDGE_Y, 20);
    print_at(PROMPT_X, PROMPT_Y, phase_prompt(p));
    put_attr_rect(0, PROMPT_Y, 20, 1, PAL_FONT);
    put_attr_rect(0, JUDGE_Y, 20, 1, PAL_FONT);
    prev_joy = joypad(); /* don't treat a held A from the previous phase as a hit */

    while (!done) {
        vsync();
        frame++;
        joy = joypad();
        pressed = (uint8_t)(joy & ~prev_joy);
        prev_joy = joy;

        if (pressed & J_SELECT) {
            want_title = 1;
            return 1;
        }

        /* bubble animation while the pot is boiling */
        if (pot_state == POT_ST_BUBBLE0 || pot_state == POT_ST_BUBBLE1) {
            if ((frame & 0x0F) == 0) {
                draw_pot((pot_state == POT_ST_BUBBLE0) ? POT_ST_BUBBLE1 : POT_ST_BUBBLE0);
            }
        }

        if (intro) {
            intro--;
            cursor_x = 0;
        } else {
            cursor_x = cursor_pos(round_no, pf);
            if (cursor_x >= GAUGE_PX) {
                cursor_x = GAUGE_PX - 1;
                late = 1;
                if (p == PHASE_TOPPING) judge = JUDGE_SKIP;
                else judge = JUDGE_MISS;
                done = 1;
            } else if (p == PHASE_TOPPING) {
                if (pressed & J_A) {
                    judge = (zone_of(cursor_x) == JUDGE_MISS) ? JUDGE_MISS : JUDGE_EGG;
                    done = 1;
                } else if (pressed & J_B) {
                    judge = (zone_of(cursor_x) == JUDGE_MISS) ? JUDGE_MISS : JUDGE_ONION;
                    done = 1;
                }
            } else if (pressed & J_A) {
                judge = zone_of(cursor_x);
                late = 0;
                done = 1;
            }
            pf++;
        }
        draw_cursor();
    }

    score += judge_points(judge);
    apply_phase_visual(p, judge, late);
    draw_hud();
    clear_row(0, JUDGE_Y, 20);
    print_at(JUDGE_X, JUDGE_Y, judge_text(judge));
    play_judge_sfx(judge);

    for (wait = 0; wait < JUDGE_FRAMES; wait++) {
        vsync();
        frame++;
        joy = joypad();
        pressed = (uint8_t)(joy & ~prev_joy);
        prev_joy = joy;
        if (pressed & J_SELECT) {
            want_title = 1;
            return 1;
        }
        if (pot_state == POT_ST_BUBBLE0 || pot_state == POT_ST_BUBBLE1) {
            if ((frame & 0x0F) == 0) {
                draw_pot((pot_state == POT_ST_BUBBLE0) ? POT_ST_BUBBLE1 : POT_ST_BUBBLE0);
            }
        }
        draw_cursor();
    }
    return 0;
}

static uint8_t rating_letter(void) {
    if (score >= 1100) return 'S';
    if (score >= 900) return 'A';
    if (score >= 700) return 'B';
    if (score >= 500) return 'C';
    return 'D';
}

static const char *done_text(void) {
    switch (doneness) {
        case DONE_PERF: return "PERFECT NOODLES";
        case DONE_OK:   return "  NICE NOODLES ";
        case DONE_HARD: return "  HARD NOODLES ";
        case DONE_SOGGY:return " SOGGY NOODLES ";
        default:        return "  RAW NOODLES  ";
    }
}

/* 0 = back to title, 1 = play again */
static uint8_t screen_results(void) {
    uint8_t joy, pressed;
    char rate[2];

    DISPLAY_OFF;
    hide_all_sprites();
    set_bkg_data(0, RESULTS_TILE_COUNT, results_tiles);
    set_bkg_data(T_RFONT, FONT_TILE_COUNT, font_tiles);
    set_bkg_data(T_RBLANK, BLANK_TILE_COUNT, blank_tiles);
    load_results_palettes();
    set_bkg_based_tiles(0, 0, 20, 18, results_map, 0);
    put_attr_rect(0, 0, 20, 18, PAL_RES);

    /* cream text plates so font tiles (cream + ink) don't sit on the wrong palette */
    {
        uint8_t row, col, tblank = T_RBLANK;
        for (row = 5; row <= 9; row += 2) {
            for (col = 2; col < 18; col++) set_bkg_tiles(col, row, 1, 1, &tblank);
        }
        for (col = 0; col < 20; col++) {
            set_bkg_tiles(col, 16, 1, 1, &tblank);
            set_bkg_tiles(col, 17, 1, 1, &tblank);
        }
    }
    put_attr_rect(2, 5, 16, 1, PAL_FONT);
    put_attr_rect(2, 7, 16, 1, PAL_FONT);
    put_attr_rect(2, 9, 16, 1, PAL_FONT);
    put_attr_rect(0, 16, 20, 2, PAL_FONT);

    print_at_base(4, 5, "RATING  ", T_RFONT);
    rate[0] = (char)rating_letter();
    rate[1] = 0;
    print_at_base(12, 5, rate, T_RFONT);

    print_at_base(4, 7, "SCORE ", T_RFONT);
    print_num(10, 7, score, 4, T_RFONT);

    print_at_base(2, 9, done_text(), T_RFONT);

    print_at_base(4, 16, "PRESS START", T_RFONT);
    print_at_base(3, 17, "SELECT: TITLE", T_RFONT);

    DISPLAY_ON;
    sfx_result();
    prev_joy = joypad();
    while (1) {
        vsync();
        joy = joypad();
        pressed = (uint8_t)(joy & ~prev_joy);
        prev_joy = joy;
        if (pressed & J_START) return 1;
        if (pressed & J_SELECT) return 0;
    }
}

/* 0 = go to title, 1 = player wants another run (from results START) */
static uint8_t play_run(void) {
    DISPLAY_OFF;
    hide_all_sprites();
    load_play_tiles();
    load_play_palettes();
    score = 0;
    has_egg = 0;
    has_onion = 0;
    doneness = DONE_RAW;
    pot_state = POT_ST_WATER;
    want_title = 0;
    prev_joy = 0;

    for (round_no = 1; round_no <= ROUND_TOTAL; round_no++) {
        has_egg = 0;
        has_onion = 0;
        doneness = DONE_RAW;
        pot_state = POT_ST_WATER;
        draw_playfield();
        DISPLAY_ON;
        for (phase = 0; phase < PHASE_COUNT; phase++) {
            if (run_phase(phase)) return 0;
        }
    }
    move_sprite(SPR_CURSOR, 0, 0);
    return screen_results();
}

/* ------------------------------------------------------------------ title */
static void press_start_visible(uint8_t on) {
    uint8_t buf[PRESS_W * PRESS_H];
    uint8_t r, c;
    if (on) {
        for (r = 0; r < PRESS_H; r++)
            for (c = 0; c < PRESS_W; c++)
                buf[r * PRESS_W + c] =
                    title_map[(PRESS_Y + r) * TITLE_MAP_W + (PRESS_X + c)];
        set_bkg_tiles(PRESS_X, PRESS_Y, PRESS_W, PRESS_H, buf);
    } else {
        for (r = 0; r < PRESS_W * PRESS_H; r++) buf[r] = T_BLANK;
        /* title tiles live at 0; blank is loaded just after them */
        for (r = 0; r < PRESS_W * PRESS_H; r++) buf[r] = TITLE_TILE_COUNT;
        set_bkg_tiles(PRESS_X, PRESS_Y, PRESS_W, PRESS_H, buf);
    }
}

static void screen_title(void) {
    uint8_t started = 0;
    uint8_t joy;

    DISPLAY_OFF;
    hide_all_sprites();
    set_bkg_data(0, TITLE_TILE_COUNT, title_tiles);
    set_bkg_data(TITLE_TILE_COUNT, BLANK_TILE_COUNT, blank_tiles);
    load_title_palettes();
    set_bkg_based_tiles(0, 0, 20, 18, title_map, 0);
    put_attr_rect(0, 0, 20, 18, PAL_TITLE);
    DISPLAY_ON;

    while (1) {
        vsync();
        frame++;
        if ((frame & 0x1F) == 0) press_start_visible((frame >> 5) & 1);
        joy = joypad();
        if (joy & J_START) started = 1;
        if (started && !(joy & J_START)) break;
    }
    press_start_visible(1);
    sfx_start();
}

/* ------------------------------------------------------------------ entry */
void main(void) {
    DISPLAY_OFF;
    SPRITES_8x8;
    SHOW_BKG;
    HIDE_WIN;
    sfx_init();
    if (DEVICE_SUPPORTS_COLOR) {
        cpu_fast();
    }
    SHOW_SPRITES;
    DISPLAY_ON;

    for (;;) {
        screen_title();
        for (;;) {
            if (!play_run()) break;
        }
    }
}
