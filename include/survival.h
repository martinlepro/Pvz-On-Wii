#ifndef PVZ_SURVIVAL_H
#define PVZ_SURVIVAL_H

#ifdef __cplusplus
extern "C" {
#endif

#include "types.h"
#include "level.h"

struct GameContext;

#define SURVIVAL_OFFSET 60
#define SURVIVAL_COUNT  11

typedef enum {
    SURVIVAL_DAY_EASY    = 0,
    SURVIVAL_DAY_HARD    = 1,
    SURVIVAL_NIGHT_EASY  = 2,
    SURVIVAL_NIGHT_HARD  = 3,
    SURVIVAL_POOL_EASY   = 4,
    SURVIVAL_POOL_HARD   = 5,
    SURVIVAL_FOG_EASY    = 6,
    SURVIVAL_FOG_HARD    = 7,
    SURVIVAL_ROOF_EASY   = 8,
    SURVIVAL_ROOF_HARD   = 9,
    SURVIVAL_ENDLESS     = 10,
} SurvivalType;

typedef enum {
    SURVIVAL_DIFF_EASY,
    SURVIVAL_DIFF_HARD,
    SURVIVAL_DIFF_ENDLESS,
} SurvivalDifficulty;

bool Survival_IsSurvival(u8 levelIndex);
SurvivalType Survival_GetType(u8 levelIndex);
World Survival_GetWorld(SurvivalType type);
SurvivalDifficulty Survival_GetDifficulty(SurvivalType type);
void Survival_Setup(struct GameContext* game);
bool Survival_BuildWavePlan(struct GameContext* game);
void Survival_OnFlagCleared(struct GameContext* game);
bool Survival_ShouldReselect(const struct GameContext* game);
u16 Survival_ExtraCost(const struct GameContext* game, PlantType type);
void Survival_GenerateNextWaves(struct GameContext* game);

#ifdef __cplusplus
}
#endif

#endif
