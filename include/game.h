/**
 * game.h - Lawn grid, planting mechanics, falling sun, and the plant tile-cursor.
 */
#ifndef PVZ_GAME_H
#define PVZ_GAME_H

#ifdef __cplusplus
extern "C" {
#endif

#include "constants.h"
#include "input.h"
#include "types.h"

typedef struct {
    GridCell  grid[GRID_ROWS][GRID_COLS];
    GameState state;

    s8        selectedSlot;    /* Seed-bar slot chosen (persists between grabs) */
    PlantType heldPlant;       /* Plant currently held (drawn semi-transparent) */
    bool      isHoldingSeed;

    s8        plantCol;        /* Tile-cursor column; may overshoot past the lawn edges */
    s8        plantRow;        /* Tile-cursor row; may overshoot past the lawn edges */

    SunDrop   suns[MAX_SUN_DROPS];
    u32       sunSpawnTimer;

    u16       sun;             /* Player currency */
    u32       frameCount;

    /* Cached layout metrics (computed at init) */
    s16 lawnX;
    s16 lawnY;
    s16 cellW;
    s16 cellH;
} GameContext;

void Game_Init(GameContext* game);
void Game_Reset(GameContext* game);

/**
 * Process seed-bar hover selection, grab/hold, tile-cursor movement (buttons
 * + Nunchuk), and release/place. Only call while the menu is closed --
 * gameplay input is fully suppressed while it is open.
 */
void Game_HandleInput(GameContext* game, const InputState* input);

/**
 * Advance passive simulation: sunflower income and falling suns (spawn,
 * fall, expire, and collection via the IR cursor). Only call while the menu
 * is closed -- the game is fully paused otherwise.
 */
void Game_Update(GameContext* game, const InputState* input);

/**
 * Drop whatever is currently held without planting it. No refund is needed
 * since the sun cost is only ever deducted on a successful placement. Call
 * this when the menu opens so a hold never lingers across a pause.
 */
void Game_CancelHold(GameContext* game);

/** True if (col,row) is inside the 9x5 lawn grid. */
bool Game_IsValidCell(s8 col, s8 row);

/**
 * Pixel top-left corner of a (possibly out-of-range) tile -- used to draw
 * the tile-cursor / held-seed preview even when it has been pushed off the
 * lawn ("far beyond the grass tiles"), so the player gets visual feedback
 * that releasing there will cancel the placement.
 */
void Game_GridToScreen(const GameContext* game, s8 col, s8 row, s16* outX, s16* outY);

/** Sun cost for a plant type; 0xFFFF if invalid. */
u16 Game_PlantCost(PlantType type);

/** Human-readable plant name for debug HUD / menu labels. */
const char* Game_PlantName(PlantType type);

#ifdef __cplusplus
}
#endif

#endif /* PVZ_GAME_H */
