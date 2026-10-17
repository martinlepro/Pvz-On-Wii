/**
 * level.h - Adventure Mode: 5 worlds x 10 levels (matches the original
 * game's structure), straight sequential progression, no level-select menu.
 * Completing a level's last wave auto-advances to the next one.
 *
 * SCOPE NOTE: wave composition is procedurally generated from a per-level
 * difficulty/unlock formula, not 50 hand-authored level designs like the
 * original. World gimmicks (Pool's water lanes, Fog's vision cone, Roof's
 * angled planting + Bungee drops) are NOT implemented -- all 5 worlds
 * currently reuse the same flat 9x5 lawn, just re-themed (background tint
 * + zombie roster). Levels 5 and 10 of each world are not yet distinct
 * mini-game / conveyor-belt levels; they play like a slightly bigger normal
 * level. These are natural next steps, not implemented yet.
 */
#ifndef PVZ_LEVEL_H
#define PVZ_LEVEL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "constants.h"
#include "types.h"
#include "zombie.h"

struct GameContext; /* defined in game.h */

typedef enum {
    WORLD_DAY = 0,
    WORLD_NIGHT,
    WORLD_POOL,
    WORLD_FOG,
    WORLD_ROOF,
} World;

#define MAX_WAVES_PER_LEVEL   10
#define MAX_ZOMBIES_PER_WAVE  10

typedef struct {
    u8         zombieCount;
    ZombieType zombieTypes[MAX_ZOMBIES_PER_WAVE];
    u8         zombieRows[MAX_ZOMBIES_PER_WAVE];
    u8         spawnedCount;   /* how many of this wave have been spawned so far */
    u32        nextSpawnTimer; /* frames until the next one spawns              */
    u32        spawnIntervalTotal; /* what nextSpawnTimer was just reset to --
                                     * lets render.cpp compute a 0..1 fill fraction
                                     * for the "zombies incoming" gauge          */
} LevelWave;

/** World for a given global level index (0-based, 0..TOTAL_LEVELS-1). */
World Level_GetWorld(u8 levelIndex);

/** True if this world's board is a daytime setting (Day/Pool/Roof). */
bool Level_IsDaytime(World world);

/** Human-readable world name, e.g. "Night" -- for the HUD level readout. */
const char* Level_WorldName(World world);

/** Flat placeholder background tint for a world (used if no background
 *  image is present at assets/backgrounds/<world>.png -- see render.cpp). */
Color4 Level_GetWorldTint(World world);

/** Build the wave plan for `levelIndex` and reset the lawn/zombies/sun ready
 *  to play it. Call once when starting a level (including auto-advance). */
void Level_Start(struct GameContext* game, u8 levelIndex);

/** Called after the player finishes selecting plants: builds the wave plan
 *  and transitions to GAME_STATE_PLAYING. */
void Level_StartPlaying(struct GameContext* game);

/** Advance wave spawning / level-complete / auto-advance-to-next-level.
 *  Call once per frame from Game_Update while GAME_STATE_PLAYING or
 *  GAME_STATE_LEVEL_WON. */
void Level_Update(struct GameContext* game);

/**
 * 0..1 fill fraction for the "zombies incoming" gauge (0 = a zombie just
 * spawned, 1 = about to spawn the next one), or a negative value if there's
 * nothing left to spawn right now (wave fully spawned and not yet cleared,
 * or the level is won/over) -- render.cpp hides the gauge in that case.
 */
f32 Level_GetSpawnGaugeProgress(const struct GameContext* game);

/**
 * 0..1 overall progress through the current level's wave set (fraction of
 * this level's total zombies already spawned across all its waves), or a
 * negative value when there's nothing meaningful to show (selection screen,
 * game over). Returns 1.0f once the level has been won. Used as the *target*
 * for the eased right-side level progress bar in render.cpp -- see
 * GameContext::levelProgressDisplayed.
 */
f32 Level_GetLevelProgress(const struct GameContext* game);

/**
 * Fills outFractions[] (must have room for at least MAX_WAVES_PER_LEVEL - 1
 * entries) with the 0..1 position of every wave boundary in the current
 * level's wave set, for the level progress bar to draw a tick mark at each
 * one. The final boundary (end of the last wave) is skipped since that's
 * just the right edge of the bar. Returns the number of fractions written.
 */
u8 Level_GetWaveTickFractions(const struct GameContext* game, f32* outFractions);

#ifdef __cplusplus
}
#endif

#endif /* PVZ_LEVEL_H */
