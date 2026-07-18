#ifndef PVZ_MINIGAME_H
#define PVZ_MINIGAME_H

#ifdef __cplusplus
extern "C" {
#endif

#include "types.h"

struct GameContext;

#define MINIGAME_OFFSET 50
#define MINIGAME_COUNT  5

typedef enum {
    MINIGAME_BIG_TROUBLE      = 0,
    MINIGAME_NIMBLE_QUICK     = 1,
    MINIGAME_COLUMN_LIKE      = 2,
    MINIGAME_INVISI_GHOUL     = 3,
    MINIGAME_LAST_STAND       = 4,
} MinigameType;

/** True if levelIndex is a minigame level. */
bool Minigame_IsMinigame(u8 levelIndex);

/** Get the minigame type from a level index. */
MinigameType Minigame_GetType(u8 levelIndex);

/** Configure the game context for minigame-specific rules.
 *  Called at Level_Start time after Game_Reset. */
void Minigame_Setup(struct GameContext* game);

/** Override the wave plan for a minigame. Return true if handled. */
bool Minigame_BuildWavePlan(struct GameContext* game);

/** Override zombie speed multiplier (1.0 = normal). */
f32 Minigame_GetSpeedMultiplier(const struct GameContext* game);

/** Check if zombie rendering is allowed. */
bool Minigame_CanRenderZombie(const struct GameContext* game);

/** Return the starting sun. */
u16 Minigame_StartingSun(const struct GameContext* game);

/** Return whether sky suns should fall. */
bool Minigame_SunFalls(const struct GameContext* game);

/** Called after a successful plant placement. */
void Minigame_OnPlantPlaced(struct GameContext* game, s8 col, s8 row, PlantType type);

/** Called when a plant is destroyed (health <= 0). */
void Minigame_OnPlantDestroyed(struct GameContext* game, PlantType type);

/** Called when a wave set is cleared (every flag in survival). */
void Minigame_OnFlagCleared(struct GameContext* game);

/** Return true if the player should reselect plants now (survival only). */
bool Minigame_ShouldReselect(const struct GameContext* game);

/** Return additional sun cost for a plant in survival mode (upgrade plant
 *  cost escalation: +50 sun per existing plant of that type on the field). */
u16 Minigame_ExtraCost(const struct GameContext* game, PlantType type);

#ifdef __cplusplus
}
#endif

#endif
