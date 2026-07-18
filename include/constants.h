/**
 * constants.h - Global game dimensions and tuning values for Wii hardware.
 * Plants vs. Zombies Wii clone (homebrew, devkitPPC / libogc / GRRLIB).
 */
#ifndef PVZ_CONSTANTS_H
#define PVZ_CONSTANTS_H

/* NTSC Wii framebuffer dimensions */
#define SCREEN_WIDTH   640
#define SCREEN_HEIGHT  480

/* Lawn grid (classic PvZ layout: 9 columns) */
#define GRID_COLS       9
#define GRID_ROWS       6    /* pool/fog use 6 rows; day/night/roof use 5 (row 5 unused) */

/* Seed bar at top of screen */
#define SEED_BAR_HEIGHT    72
#define SEED_SLOT_COUNT    8
#define SEED_SLOT_WIDTH    52
#define SEED_SLOT_HEIGHT   56
#define SEED_BAR_PADDING   8

/* Lawn occupies remaining vertical space, centered horizontally */
#define LAWN_MARGIN_X      40
#define LAWN_MARGIN_Y      (SEED_BAR_HEIGHT + 8)

/* Derived cell size (computed once at runtime in Game_Init) */
#define CELL_WIDTH_DEFAULT  ((SCREEN_WIDTH - LAWN_MARGIN_X * 2) / GRID_COLS)
#define CELL_HEIGHT_DEFAULT ((SCREEN_HEIGHT - LAWN_MARGIN_Y - 16) / GRID_ROWS)

/*
 * Plant tile-cursor (the position the held seed will be placed at).
 * Moved by Button 1 / Button 2 (left-right) and, if a Nunchuk is attached,
 * by the analog stick (all four directions), one tile per press/flick.
 * A small overshoot band is allowed past the lawn edge so the cursor can be
 * pushed "off the grass" -- releasing there cancels the placement.
 */
#define PLANT_CURSOR_OVERSHOOT  2
#define PLANT_CURSOR_COL_MIN    (-PLANT_CURSOR_OVERSHOOT)
#define PLANT_CURSOR_COL_MAX    (GRID_COLS - 1 + PLANT_CURSOR_OVERSHOOT)
#define PLANT_CURSOR_ROW_MIN    (-PLANT_CURSOR_OVERSHOOT)
#define PLANT_CURSOR_ROW_MAX    (GRID_ROWS - 1 + PLANT_CURSOR_OVERSHOOT)

/* Nunchuk discrete tile-move thresholds (uses calibrated joystick_t.mag/.ang) */
#define TILE_MOVE_ARM_MAG       0.55f  /* magnitude required to trigger a move   */
#define TILE_MOVE_REARM_MAG     0.25f  /* must fall back below this to re-arm    */

/* Held-seed transparency while A/B is held down (0-255 alpha) */
#define HELD_SEED_ALPHA         150

/* Falling sun (collected by pointing the Wiimote IR cursor at it) */
#define MAX_SUN_DROPS               6
#define SUN_VALUE                   25
#define SUN_SPAWN_INTERVAL_FRAMES   (TARGET_FPS * 9)
#define SUN_FALL_SPEED              1.0f
#define SUN_COLLECT_RADIUS          22.0f
#define SUN_LIFETIME_FRAMES         (TARGET_FPS * 8)
#define SUN_ICON_SIZE               32

/* In-game options menu (opened with - or +) */
#define MENU_PANEL_W            360
#define MENU_PANEL_H            260
#define MENU_ITEM_H             28

/* Persisted settings file (SD card; written on menu close) */
#define SETTINGS_DIR              "sd:/apps/pvz_wii"
#define SETTINGS_PATH             "sd:/apps/pvz_wii/settings.cfg"

/* Starting sun currency */
#define STARTING_SUN           150

/* Plant costs (sun) */
#define COST_PEASHOOTER        100
#define COST_SUNFLOWER         50
#define COST_WALLNUT           50
#define COST_CHERRY_BOMB       150
#define COST_REPEATER          200
#define COST_SNOW_PEA          175

/* Frame timing */
#define TARGET_FPS             60
#define FRAME_TIME_US          (1000000 / TARGET_FPS)

/* Cursor visual */
#define CURSOR_SIZE            24

/*---------------------------------------------------------------------------
 * Zombies, combat, and Adventure Mode.
 *
 * Stat numbers below are reasonable, internally-consistent approximations
 * (relative scale), not a claim to match the original game's exact point
 * values -- tune freely.
 *-------------------------------------------------------------------------*/
#define MAX_ZOMBIES             40
#define MAX_PROJECTILES         48

/* Zombie movement: tile-widths per second walking across the lawn. */
#define ZOMBIE_BASE_SPEED_TPS   0.28f
#define ZOMBIE_EAT_INTERVAL_FRAMES   (TARGET_FPS / 2)   /* one bite every 0.5s   */
#define ZOMBIE_DEATH_FADE_FRAMES     (TARGET_FPS)       /* death-poof duration   */
#define ZOMBIE_SPAWN_COL_OFFSET      1.4f               /* how far off-screen right zombies spawn */

/* Time between one zombie spawning and the next being due -- "usually after
 * 5 seconds", with a little jitter so it doesn't feel metronomic. Also used
 * for the very first zombie of each wave. See the spawn gauge in render.cpp. */
#define ZOMBIE_SPAWN_INTERVAL_FRAMES       (TARGET_FPS * 5)
#define ZOMBIE_SPAWN_INTERVAL_JITTER_FRAMES (TARGET_FPS)   /* +/- up to 1s */

/* "Zombies incoming" gauge at the bottom of the screen -- fills up over the
 * next ZOMBIE_SPAWN_INTERVAL_FRAMES-ish and resets when a zombie spawns. */
#define SPAWN_GAUGE_WIDTH       220
#define SPAWN_GAUGE_HEIGHT      16
#define SPAWN_GAUGE_MARGIN_Y    10   /* gap from the bottom of the screen */

/* Pea / plant attacks */
#define PEA_SPEED_TPS           6.0f
#define PEA_DAMAGE              20
#define SNOW_PEA_SLOW_FRAMES    (TARGET_FPS * 2)
#define SNOW_PEA_SLOW_FACTOR    0.5f
#define ICE_FREEZE_FRAMES       (TARGET_FPS * 5)   /* Ice-shroom freeze duration */

/* Shooter cooldowns in frames */
#define SHOOTER_COOLDOWN_FRAMES      (TARGET_FPS * 3 / 2)  /* ~1.5s */
#define REPEATER_COOLDOWN_FRAMES     (TARGET_FPS)           /* ~1s */
#define GATLING_COOLDOWN_FRAMES      (TARGET_FPS * 3 / 4)  /* ~0.75s */
#define THREEPEATER_COOLDOWN_FRAMES  (TARGET_FPS * 3 / 2)
#define SPLIT_COOLDOWN_FRAMES        (TARGET_FPS * 3 / 2)
#define STARFRUIT_COOLDOWN_FRAMES    (TARGET_FPS * 3 / 2)
#define LOB_COOLDOWN_FRAMES          (TARGET_FPS * 7 / 4)  /* ~1.75s for cabbage/melon */
#define FUME_COOLDOWN_FRAMES         (TARGET_FPS)
#define SPORE_COOLDOWN_FRAMES        (TARGET_FPS)
#define GLOOM_INITIAL_COOLDOWN_FRAMES 12                     /* 0.2s */
#define GLOOM_COOLDOWN_FRAMES        (TARGET_FPS * 115 / 60) /* ~1.925s */
#define CACTUS_COOLDOWN_FRAMES       (TARGET_FPS * 3 / 2)
#define CATTAL_COOLDOWN_FRAMES       (TARGET_FPS * 3 / 2)

/* Projectile range (tiles before it expires) */
#define SPORE_RANGE_TILES       3.5f
#define FUME_RANGE_TILES        7.0f
#define PEA_RANGE_TILES         9.0f

/* Instant-kill damage threshold for Cherry Bomb, Doom-shroom, Jalapeno etc. */
#define CHERRY_BOMB_DAMAGE      1800
#define CHERRY_BOMB_RADIUS_TILES 1
#define DOOM_SHROOM_RADIUS_TILES 3
#define JALAPENO_DAMAGE         1800

/* Butter stun duration (Kernel-pult) */
#define BUTTER_STUN_FRAMES      (TARGET_FPS * 5)

/* Adventure Mode structure (matches the original: 5 worlds x 10 levels = 50) */
#define WORLD_COUNT             5
#define LEVELS_PER_WORLD        10
#define TOTAL_LEVELS            (WORLD_COUNT * LEVELS_PER_WORLD)
#define LEVEL_INTERMISSION_FRAMES (TARGET_FPS * 3)  /* pause on win before next level loads */

#endif /* PVZ_CONSTANTS_H */
