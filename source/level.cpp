/**
 * level.cpp - see level.h for the design and scope notes.
 */
#include <string.h>
#include <stdlib.h>
#include "level.h"
#include "game.h"
#include "audio.h"
#include "combat.h"
#include "minigame.h"
#include "survival.h"

World Level_GetWorld(u8 levelIndex) {
    return (World)((levelIndex / LEVELS_PER_WORLD) % WORLD_COUNT);
}

bool Level_IsDaytime(World world) {
    return world == WORLD_DAY || world == WORLD_POOL || world == WORLD_ROOF;
}

const char* Level_WorldName(World world) {
    switch (world) {
        case WORLD_DAY:  return "Day";
        case WORLD_NIGHT:return "Night";
        case WORLD_POOL: return "Pool";
        case WORLD_FOG:  return "Fog";
        case WORLD_ROOF: return "Roof";
        default:         return "?";
    }
}

Color4 Level_GetWorldTint(World world) {
    switch (world) {
        case WORLD_DAY:   return (Color4){ 138, 197, 92,  255 }; /* bright grass, daylight   */
        case WORLD_NIGHT: return (Color4){  42,  58, 74,  255 }; /* dark desaturated lawn    */
        case WORLD_POOL:  return (Color4){ 108, 178, 176, 255 }; /* grass + pool, daytime    */
        case WORLD_FOG:   return (Color4){  58,  66, 78,  255 }; /* pool at night, foggy     */
        case WORLD_ROOF:  return (Color4){ 196, 142, 92,  255 }; /* rooftop shingles, dusk   */
        default:          return (Color4){ 120, 120, 120, 255 };
    }
}

/* Cumulative zombie unlock schedule: a type becomes available in the random
 * wave pool from `unlockLevel` (0-based global level index) onward. This is
 * a reasonable approximation of the original's pacing, not a transcription
 * of its exact unlock levels. */
typedef struct { u8 unlockLevel; ZombieType type; } UnlockEntry;

static const UnlockEntry kUnlockSchedule[] = {
    { 0,  ZOMBIE_BASIC },
    { 2,  ZOMBIE_FLAG },
    { 3,  ZOMBIE_CONEHEAD },
    { 6,  ZOMBIE_BUCKETHEAD },
    { 8,  ZOMBIE_POLE_VAULTING },

    { 10, ZOMBIE_NEWSPAPER },
    { 12, ZOMBIE_SCREENDOOR },
    { 15, ZOMBIE_DISCO },
    { 15, ZOMBIE_BACKUP_DANCER },
    { 18, ZOMBIE_FOOTBALL },

    { 20, ZOMBIE_SNORKEL },
    { 22, ZOMBIE_DOLPHIN_RIDER },
    { 25, ZOMBIE_JACK_IN_BOX },
    { 27, ZOMBIE_BALLOON },
    { 28, ZOMBIE_BASEBALL },

    { 30, ZOMBIE_MINER },
    { 32, ZOMBIE_POGO },
    { 34, ZOMBIE_CATAPULT },
    { 36, ZOMBIE_GARGANTUAR },
    { 37, ZOMBIE_IMP },
    { 38, ZOMBIE_TRASH },
    { 39, ZOMBIE_CIBLE },

    { 40, ZOMBIE_RED_GARGANTUAR },
    { 42, ZOMBIE_GIGA_FOOTBALL },
    { 44, ZOMBIE_ZOMBONI },
    { 46, ZOMBIE_YETI },
    { 47, ZOMBIE_ZOMBIE_PEASHOOTER },
    { 47, ZOMBIE_ZOMBIE_WALLNUT },
    { 48, ZOMBIE_ZOMBIE_TALLNUT },
    { 48, ZOMBIE_ZOMBIE_SQUASH },
    /* ZOMBIE_ZOMBOSS is not part of the random pool -- it's placed as a
     * guaranteed single spawn on the final level (49) instead. */
};
#define UNLOCK_SCHEDULE_COUNT (sizeof(kUnlockSchedule) / sizeof(kUnlockSchedule[0]))

static int BuildUnlockedPool(u8 levelIndex, ZombieType* outPool, int maxOut) {
    int count = 0;
    for (unsigned i = 0; i < UNLOCK_SCHEDULE_COUNT && count < maxOut; i++) {
        if (kUnlockSchedule[i].unlockLevel <= levelIndex) {
            outPool[count++] = kUnlockSchedule[i].type;
        }
    }
    if (count == 0) {
        outPool[0] = ZOMBIE_BASIC;
        count = 1;
    }
    return count;
}

/* Builds the wave plan for `levelIndex` directly into game->waves[]. */
void BuildWavePlan(GameContext* game, u8 levelIndex) {
    ZombieType pool[UNLOCK_SCHEDULE_COUNT];
    int poolSize = BuildUnlockedPool(levelIndex, pool, UNLOCK_SCHEDULE_COUNT);

    u8 levelInWorld = levelIndex % LEVELS_PER_WORLD; /* 0-9 */
    bool isFinale = (levelInWorld == LEVELS_PER_WORLD - 1); /* "10th level" -- biggest wave */
    bool isBossLevel = (levelIndex == TOTAL_LEVELS - 1);    /* 5-10: Dr. Zomboss            */

    game->waveCount = isFinale ? 4 : 3;
    if (game->waveCount > MAX_WAVES_PER_LEVEL) game->waveCount = MAX_WAVES_PER_LEVEL;

    /* Overall difficulty grows slowly across all 50 levels. */
    u8 baseCount = 2 + (levelIndex / 5);
    if (baseCount > MAX_ZOMBIES_PER_WAVE - 2) baseCount = MAX_ZOMBIES_PER_WAVE - 2;

    for (u8 w = 0; w < game->waveCount; w++) {
        LevelWave* wave = &game->waves[w];
        memset(wave, 0, sizeof(LevelWave));

        bool isLastWave = (w == game->waveCount - 1);
        u8 count = baseCount + (isLastWave ? 2 : 0);
        if (count > MAX_ZOMBIES_PER_WAVE) count = MAX_ZOMBIES_PER_WAVE;

        for (u8 i = 0; i < count; i++) {
            ZombieType t = pool[rand() % poolSize];
            wave->zombieTypes[i] = t;
            wave->zombieRows[i] = (u8)(rand() % GRID_ROWS);
        }
        wave->zombieCount = count;
        wave->spawnedCount = 0;
        wave->spawnIntervalTotal = ZOMBIE_SPAWN_INTERVAL_FRAMES
            + (rand() % (2 * ZOMBIE_SPAWN_INTERVAL_JITTER_FRAMES + 1)) - ZOMBIE_SPAWN_INTERVAL_JITTER_FRAMES;
        wave->nextSpawnTimer = wave->spawnIntervalTotal;
    }

    if (isBossLevel) {
        /* Guarantee a single Dr. Zomboss in the final wave, in the middle row. */
        LevelWave* finalWave = &game->waves[game->waveCount - 1];
        if (finalWave->zombieCount < MAX_ZOMBIES_PER_WAVE) {
            finalWave->zombieTypes[finalWave->zombieCount] = ZOMBIE_ZOMBOSS;
            finalWave->zombieRows[finalWave->zombieCount] = GRID_ROWS / 2;
            finalWave->zombieCount++;
        }
    }
}

void Level_Start(GameContext* game, u8 levelIndex) {
    game->currentLevelIndex = levelIndex;
    game->currentWave = 0;
    game->levelIntermissionTimer = 0;

    bool isSurvival = Survival_IsSurvival(levelIndex);

    if (isSurvival) {
        Survival_Setup(game);
    } else if (Minigame_IsMinigame(levelIndex)) {
        Minigame_Setup(game);
    } else {
        if (levelIndex >= TOTAL_LEVELS) levelIndex = 0;
        game->currentLevelIndex = levelIndex;

        World w = Level_GetWorld(levelIndex);
        game->worldFlags = WORLD_FLAG_SUN_FALLS;
        game->rowCount = 5;
        switch (w) {
            case WORLD_NIGHT:
                game->worldFlags = WORLD_FLAG_HAS_GRAVES;
                game->rowCount = 5;
                break;
            case WORLD_POOL:
                game->worldFlags = WORLD_FLAG_SUN_FALLS | WORLD_FLAG_HAS_WATER;
                game->rowCount = 6;
                break;
            case WORLD_FOG:
                game->worldFlags = WORLD_FLAG_HAS_WATER | WORLD_FLAG_HAS_FOG;
                game->rowCount = 6;
                break;
            case WORLD_ROOF:
                game->worldFlags = WORLD_FLAG_SUN_FALLS | WORLD_FLAG_NEEDS_POTS;
                game->rowCount = 5;
                break;
            default:
                break;
        }
    }

    Game_Reset(game);
    Zombie_ResetAll(game->zombies);
    Combat_ResetAll(game);

    /* Populate graves for Night levels */
    if (game->worldFlags & WORLD_FLAG_HAS_GRAVES)
    {
        for (s8 row = 0; row < (s8)game->rowCount; ++row)
        {
            for (s8 col = 0; col < GRID_COLS; ++col)
            {
                if ((rand() % 10) < 4 && !(row == 0 && col == 0))
                    game->grid[row][col].hasGrave = true;
            }
        }
    }

    /* Build unlocked plant list and go to selection screen */
    Plant_GetUnlockedForLevel(levelIndex, game->unlockedPlants, &game->unlockedCount);
    game->selectedCount = 0;
    game->selectionCursor = 0;
    game->state = GAME_STATE_SELECTING;

    /* Start level-appropriate background music */
    Audio_PlayLevelBGM(levelIndex);
}

static void SpawnDueZombiesForWave(GameContext* game, LevelWave* wave) {
    if (wave->spawnedCount >= wave->zombieCount) return;

    if (wave->nextSpawnTimer > 0) {
        wave->nextSpawnTimer--;
        return;
    }

    ZombieType t = wave->zombieTypes[wave->spawnedCount];
    u8 row = wave->zombieRows[wave->spawnedCount];
    Zombie_Spawn(game, t, row);
    wave->spawnedCount++;

    /* "Usually after 5 seconds" -- a little jitter so it doesn't feel
     * metronomic. Stored in spawnIntervalTotal too so the gauge in
     * render.cpp knows what 100% full corresponds to. */
    wave->spawnIntervalTotal = ZOMBIE_SPAWN_INTERVAL_FRAMES
        + (rand() % (2 * ZOMBIE_SPAWN_INTERVAL_JITTER_FRAMES + 1)) - ZOMBIE_SPAWN_INTERVAL_JITTER_FRAMES;
    wave->nextSpawnTimer = wave->spawnIntervalTotal;
}

void Level_StartPlaying(GameContext* game) {
    if (Survival_IsSurvival(game->currentLevelIndex)) {
        Survival_BuildWavePlan(game);
    } else if (Minigame_IsMinigame(game->currentLevelIndex)) {
        Minigame_BuildWavePlan(game);
    } else {
        BuildWavePlan(game, game->currentLevelIndex);
    }
    game->state = GAME_STATE_PLAYING;
}

void Level_Update(GameContext* game) {
    if (game->state == GAME_STATE_LEVEL_WON)
        return;

    if (game->state != GAME_STATE_PLAYING) return;

    LevelWave* wave = &game->waves[game->currentWave];
    SpawnDueZombiesForWave(game, wave);

    bool waveFullySpawned = (wave->spawnedCount >= wave->zombieCount);
    bool waveCleared = waveFullySpawned && Zombie_NoneActive(game->zombies);

    if (waveCleared) {
        if (game->currentWave + 1 < game->waveCount) {
            game->currentWave++;
        } else {
            /* Last wave of the set cleared */
            if (Survival_IsSurvival(game->currentLevelIndex) &&
                Survival_GetDifficulty(Survival_GetType(game->currentLevelIndex)) == SURVIVAL_DIFF_ENDLESS)
            {
                game->currentWave = 0;
                Survival_OnFlagCleared(game);
            }
            else if (Survival_IsSurvival(game->currentLevelIndex))
            {
                /* Non-endless survival: all waves cleared -> victory */
                game->state = GAME_STATE_LEVEL_WON;
                game->levelIntermissionTimer = LEVEL_INTERMISSION_FRAMES;
            }
            else
            {
                game->state = GAME_STATE_LEVEL_WON;
                game->levelIntermissionTimer = LEVEL_INTERMISSION_FRAMES;
            }
        }
    }
}

f32 Level_GetSpawnGaugeProgress(const GameContext* game) {
    if (game->state != GAME_STATE_PLAYING) return -1.0f;

    const LevelWave* wave = &game->waves[game->currentWave];
    if (wave->spawnedCount >= wave->zombieCount) return -1.0f; /* nothing left to spawn this wave */
    if (wave->spawnIntervalTotal == 0) return -1.0f;           /* guard against div-by-zero        */

    f32 progress = 1.0f - (f32)wave->nextSpawnTimer / (f32)wave->spawnIntervalTotal;
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;
    return progress;
}
