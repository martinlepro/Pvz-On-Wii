/**
 * constants.h - Global game dimensions and tuning values for Wii hardware.
 * Plants vs. Zombies Wii clone (homebrew, devkitPPC / libogc / GRRLIB).
 */
#ifndef PVZ_CONSTANTS_H
#define PVZ_CONSTANTS_H

/* NTSC Wii framebuffer dimensions */
#define SCREEN_WIDTH   640
#define SCREEN_HEIGHT  480

/* Lawn grid (classic PvZ layout: 9 columns x 5 rows) */
#define GRID_COLS      9
#define GRID_ROWS      5

/* Seed bar at top of screen */
#define SEED_BAR_HEIGHT    72
#define SEED_SLOT_COUNT    6
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

#endif /* PVZ_CONSTANTS_H */
