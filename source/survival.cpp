#include "survival.h"
#include "game.h"
#include "level.h"
#include "plant.h"
#include <stdlib.h>
#include <string.h>

bool Survival_IsSurvival(u8 levelIndex)
{
    return levelIndex >= SURVIVAL_OFFSET && levelIndex < SURVIVAL_OFFSET + SURVIVAL_COUNT;
}

SurvivalType Survival_GetType(u8 levelIndex)
{
    if (!Survival_IsSurvival(levelIndex))
        return SURVIVAL_DAY_EASY;
    return (SurvivalType)(levelIndex - SURVIVAL_OFFSET);
}

World Survival_GetWorld(SurvivalType type)
{
    switch (type)
    {
        case SURVIVAL_DAY_EASY:
        case SURVIVAL_DAY_HARD:
            return WORLD_DAY;
        case SURVIVAL_NIGHT_EASY:
        case SURVIVAL_NIGHT_HARD:
            return WORLD_NIGHT;
        case SURVIVAL_POOL_EASY:
        case SURVIVAL_POOL_HARD:
            return WORLD_POOL;
        case SURVIVAL_FOG_EASY:
        case SURVIVAL_FOG_HARD:
            return WORLD_FOG;
        case SURVIVAL_ROOF_EASY:
        case SURVIVAL_ROOF_HARD:
            return WORLD_ROOF;
        case SURVIVAL_ENDLESS:
            return WORLD_POOL;
        default:
            return WORLD_DAY;
    }
}

SurvivalDifficulty Survival_GetDifficulty(SurvivalType type)
{
    switch (type)
    {
        case SURVIVAL_DAY_EASY:
        case SURVIVAL_NIGHT_EASY:
        case SURVIVAL_POOL_EASY:
        case SURVIVAL_FOG_EASY:
        case SURVIVAL_ROOF_EASY:
            return SURVIVAL_DIFF_EASY;
        case SURVIVAL_DAY_HARD:
        case SURVIVAL_NIGHT_HARD:
        case SURVIVAL_POOL_HARD:
        case SURVIVAL_FOG_HARD:
        case SURVIVAL_ROOF_HARD:
            return SURVIVAL_DIFF_HARD;
        case SURVIVAL_ENDLESS:
            return SURVIVAL_DIFF_ENDLESS;
        default:
            return SURVIVAL_DIFF_EASY;
    }
}

void Survival_Setup(GameContext* game)
{
    if (!game) return;
    SurvivalType type = Survival_GetType(game->currentLevelIndex);
    World world = Survival_GetWorld(type);

    game->survivalFlags = 0;
    game->wavesSinceReselect = 0;
    game->survivalWaveSet = 0;
    game->isSurvivalReselect = false;

    switch (world)
    {
        case WORLD_DAY:
            game->worldFlags = WORLD_FLAG_SUN_FALLS;
            game->rowCount = 5;
            game->sun = 150;
            break;

        case WORLD_NIGHT:
            game->worldFlags = WORLD_FLAG_HAS_GRAVES;
            game->rowCount = 5;
            game->sun = 150;
            break;

        case WORLD_POOL:
            game->worldFlags = WORLD_FLAG_SUN_FALLS | WORLD_FLAG_HAS_WATER;
            game->rowCount = 6;
            game->sun = 150;
            break;

        case WORLD_FOG:
            game->worldFlags = WORLD_FLAG_HAS_WATER | WORLD_FLAG_HAS_FOG;
            game->rowCount = 6;
            game->sun = 150;
            break;

        case WORLD_ROOF:
            game->worldFlags = WORLD_FLAG_SUN_FALLS | WORLD_FLAG_NEEDS_POTS;
            game->rowCount = 5;
            game->sun = 150;
            break;
    }
}

bool Survival_BuildWavePlan(GameContext* game)
{
    if (!game) return false;
    SurvivalType type = Survival_GetType(game->currentLevelIndex);
    SurvivalDifficulty diff = Survival_GetDifficulty(type);
    u8 rowCount = game->rowCount;

    switch (diff)
    {
        case SURVIVAL_DIFF_EASY:
        {
            ZombieType pool[] = {
                ZOMBIE_BASIC, ZOMBIE_CONEHEAD, ZOMBIE_FLAG,
                ZOMBIE_BUCKETHEAD, ZOMBIE_NEWSPAPER, ZOMBIE_SCREENDOOR,
            };
            int poolSize = sizeof(pool) / sizeof(pool[0]);

            game->waveCount = 5;
            for (u8 w = 0; w < game->waveCount; w++)
            {
                LevelWave* wave = &game->waves[w];
                memset(wave, 0, sizeof(LevelWave));
                u8 count = 3 + w;
                if (count > MAX_ZOMBIES_PER_WAVE) count = MAX_ZOMBIES_PER_WAVE;
                for (u8 i = 0; i < count; i++)
                {
                    int idx = rand() % poolSize;
                    wave->zombieTypes[i] = pool[idx];
                    wave->zombieRows[i] = (u8)(rand() % rowCount);
                }
                wave->zombieCount = count;
                wave->spawnedCount = 0;
                wave->spawnIntervalTotal = 180 - w * 10;
                if (wave->spawnIntervalTotal < 90) wave->spawnIntervalTotal = 90;
                wave->nextSpawnTimer = wave->spawnIntervalTotal;
            }
            return true;
        }

        case SURVIVAL_DIFF_HARD:
        {
            ZombieType pool[] = {
                ZOMBIE_BASIC, ZOMBIE_CONEHEAD, ZOMBIE_FLAG,
                ZOMBIE_BUCKETHEAD, ZOMBIE_NEWSPAPER, ZOMBIE_FOOTBALL,
                ZOMBIE_SCREENDOOR, ZOMBIE_POLE_VAULTING, ZOMBIE_SNORKEL,
                ZOMBIE_DOLPHIN_RIDER, ZOMBIE_JACK_IN_BOX, ZOMBIE_BALLOON,
                ZOMBIE_POGO, ZOMBIE_MINER, ZOMBIE_CATAPULT,
                ZOMBIE_GARGANTUAR, ZOMBIE_IMP,
            };
            int poolSize = sizeof(pool) / sizeof(pool[0]);

            game->waveCount = 10;
            for (u8 w = 0; w < game->waveCount; w++)
            {
                LevelWave* wave = &game->waves[w];
                memset(wave, 0, sizeof(LevelWave));
                u8 count = 4 + w / 2;
                if (count > MAX_ZOMBIES_PER_WAVE) count = MAX_ZOMBIES_PER_WAVE;
                int usablePool = poolSize;
                if (w < 3) usablePool = 6;
                else if (w < 6) usablePool = 12;
                for (u8 i = 0; i < count; i++)
                {
                    int idx = rand() % usablePool;
                    ZombieType t = pool[idx];
                    if (w < 3 && (t == ZOMBIE_GARGANTUAR || t == ZOMBIE_IMP))
                        t = pool[rand() % 3];
                    wave->zombieTypes[i] = t;
                    wave->zombieRows[i] = (u8)(rand() % rowCount);
                }
                wave->zombieCount = count;
                wave->spawnedCount = 0;
                wave->spawnIntervalTotal = 150 - w * 5;
                if (wave->spawnIntervalTotal < 60) wave->spawnIntervalTotal = 60;
                wave->nextSpawnTimer = wave->spawnIntervalTotal;
            }
            return true;
        }

        case SURVIVAL_DIFF_ENDLESS:
        {
            Survival_GenerateNextWaves(game);
            return true;
        }
    }
    return false;
}

void Survival_OnFlagCleared(GameContext* game)
{
    if (!game) return;

    /* Only ever called for Endless (see Level_Update in level.cpp): Easy and
     * Hard survival have a fixed 5/10-wave plan built up front, so their
     * per-flag reselect and their end-of-run victory are handled directly
     * there instead of through this flag-cycling counter. */
    game->survivalFlags++;
    game->wavesSinceReselect++;

    if (game->wavesSinceReselect >= 2)
    {
        game->wavesSinceReselect = 0;
        game->isSurvivalReselect = true;
    }
}

bool Survival_ShouldReselect(const GameContext* game)
{
    if (!game) return false;
    return game->isSurvivalReselect;
}

u16 Survival_ExtraCost(const GameContext* game, PlantType type)
{
    if (!game) return 0;
    if (!Survival_IsSurvival(game->currentLevelIndex))
        return 0;

    const PlantDef* def = Plant_GetDef(type);
    if (!def) return 0;
    if (def->recharge >= 3.0f && def->sunCost >= 200)
    {
        u16 count = 0;
        for (s8 r = 0; r < (s8)game->rowCount; r++)
            for (s8 c = 0; c < GRID_COLS; c++)
                if (game->grid[r][c].plant == type)
                    count++;
        return count * 50;
    }
    return 0;
}

void Survival_GenerateNextWaves(GameContext* game)
{
    if (!game) return;

    u8 set = game->survivalWaveSet;
    u8 rowCount = game->rowCount;

    game->waveCount = 3;

    ZombieType basePool[] = {
        ZOMBIE_BASIC, ZOMBIE_CONEHEAD, ZOMBIE_FLAG,
        ZOMBIE_BUCKETHEAD, ZOMBIE_NEWSPAPER, ZOMBIE_FOOTBALL,
        ZOMBIE_SNORKEL, ZOMBIE_DOLPHIN_RIDER, ZOMBIE_POLE_VAULTING,
        ZOMBIE_SCREENDOOR, ZOMBIE_JACK_IN_BOX, ZOMBIE_BALLOON,
        ZOMBIE_POGO, ZOMBIE_MINER, ZOMBIE_CATAPULT,
        ZOMBIE_GARGANTUAR, ZOMBIE_RED_GARGANTUAR, ZOMBIE_ZOMBONI,
        ZOMBIE_DISCO, ZOMBIE_IMP,
    };
    int basePoolSize = sizeof(basePool) / sizeof(basePool[0]);

    for (u8 w = 0; w < game->waveCount; w++)
    {
        LevelWave* wave = &game->waves[w];
        memset(wave, 0, sizeof(LevelWave));

        u8 count = 3 + set + w * 2;
        if (count > MAX_ZOMBIES_PER_WAVE) count = MAX_ZOMBIES_PER_WAVE;

        for (u8 i = 0; i < count; i++)
        {
            int poolSize = 3 + set;
            if (poolSize > basePoolSize) poolSize = basePoolSize;

            int idx = rand() % poolSize;
            ZombieType t = basePool[idx];

            if (w == 0 && (t == ZOMBIE_GARGANTUAR || t == ZOMBIE_RED_GARGANTUAR))
            {
                t = basePool[rand() % (poolSize > 3 ? 3 : poolSize)];
            }

            wave->zombieTypes[i] = t;
            wave->zombieRows[i] = (u8)(rand() % rowCount);
        }

        wave->zombieCount = count;
        wave->spawnedCount = 0;
        wave->spawnIntervalTotal = 180 - (set * 5);
        if (wave->spawnIntervalTotal < 60) wave->spawnIntervalTotal = 60;
        wave->nextSpawnTimer = wave->spawnIntervalTotal;
    }

    game->survivalWaveSet++;
}
