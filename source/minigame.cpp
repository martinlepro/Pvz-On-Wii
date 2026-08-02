#include "minigame.h"
#include "game.h"
#include "level.h"
#include "plant.h"
#include <stdlib.h>
#include <string.h>

bool Minigame_IsMinigame(u8 levelIndex)
{
    return levelIndex >= MINIGAME_OFFSET && levelIndex < MINIGAME_OFFSET + MINIGAME_COUNT;
}

MinigameType Minigame_GetType(u8 levelIndex)
{
    if (!Minigame_IsMinigame(levelIndex))
        return MINIGAME_BIG_TROUBLE;
    return (MinigameType)(levelIndex - MINIGAME_OFFSET);
}

void Minigame_Setup(GameContext* game)
{
    if (!game) return;
    MinigameType mg = Minigame_GetType(game->currentLevelIndex);

    switch (mg)
    {
        case MINIGAME_BIG_TROUBLE:
            game->worldFlags = WORLD_FLAG_SUN_FALLS;
            game->rowCount = 5;
            game->sun = STARTING_SUN;
            break;

        case MINIGAME_NIMBLE_QUICK:
            game->worldFlags = WORLD_FLAG_SUN_FALLS;
            game->rowCount = 5;
            game->sun = STARTING_SUN;
            break;

        case MINIGAME_COLUMN_LIKE:
            game->worldFlags = WORLD_FLAG_SUN_FALLS;
            game->rowCount = 5;
            game->sun = STARTING_SUN;
            break;

        case MINIGAME_INVISI_GHOUL:
            game->worldFlags = WORLD_FLAG_HAS_GRAVES;
            game->rowCount = 5;
            game->sun = STARTING_SUN;
            break;

        case MINIGAME_LAST_STAND:
            game->worldFlags = WORLD_FLAG_SUN_FALLS;
            game->rowCount = 5;
            game->sun = 5000;
            break;
    }
}

bool Minigame_BuildWavePlan(GameContext* game)
{
    if (!game) return false;
    MinigameType mg = Minigame_GetType(game->currentLevelIndex);
    u8 rowCount = game->rowCount;

    switch (mg)
    {
        case MINIGAME_BIG_TROUBLE:
        {
            /* More zombies, lower health (smaller). Use basic zombies only. */
            game->waveCount = 3;
            for (u8 w = 0; w < game->waveCount; w++)
            {
                LevelWave* wave = &game->waves[w];
                memset(wave, 0, sizeof(LevelWave));
                u8 count = 6 + w * 4;
                if (count > MAX_ZOMBIES_PER_WAVE) count = MAX_ZOMBIES_PER_WAVE;
                for (u8 i = 0; i < count; i++)
                {
                    wave->zombieTypes[i] = ZOMBIE_BASIC;
                    wave->zombieRows[i] = (u8)(rand() % rowCount);
                }
                wave->zombieCount = count;
                wave->spawnedCount = 0;
                wave->spawnIntervalTotal = 180;
                wave->nextSpawnTimer = wave->spawnIntervalTotal;
            }
            return true;
        }

        case MINIGAME_NIMBLE_QUICK:
        {
            game->waveCount = 3;
            for (u8 w = 0; w < game->waveCount; w++)
            {
                LevelWave* wave = &game->waves[w];
                memset(wave, 0, sizeof(LevelWave));
                u8 count = 3 + w * 2;
                if (count > MAX_ZOMBIES_PER_WAVE) count = MAX_ZOMBIES_PER_WAVE;
                for (u8 i = 0; i < count; i++)
                {
                    ZombieType pool[] = { ZOMBIE_BASIC, ZOMBIE_CONEHEAD, ZOMBIE_FLAG };
                    wave->zombieTypes[i] = pool[rand() % 3];
                    wave->zombieRows[i] = (u8)(rand() % rowCount);
                }
                wave->zombieCount = count;
                wave->spawnedCount = 0;
                wave->spawnIntervalTotal = 120;
                wave->nextSpawnTimer = wave->spawnIntervalTotal;
            }
            return true;
        }

        case MINIGAME_COLUMN_LIKE:
        {
            /* Fewer zombies; level is about planting columns. */
            game->waveCount = 4;
            for (u8 w = 0; w < game->waveCount; w++)
            {
                LevelWave* wave = &game->waves[w];
                memset(wave, 0, sizeof(LevelWave));
                u8 count = 2 + w;
                if (count > MAX_ZOMBIES_PER_WAVE) count = MAX_ZOMBIES_PER_WAVE;
                for (u8 i = 0; i < count; i++)
                {
                    ZombieType pool[] = { ZOMBIE_BASIC, ZOMBIE_CONEHEAD, ZOMBIE_BUCKETHEAD };
                    wave->zombieTypes[i] = pool[rand() % 3];
                    wave->zombieRows[i] = (u8)(rand() % rowCount);
                }
                wave->zombieCount = count;
                wave->spawnedCount = 0;
                wave->spawnIntervalTotal = 300;
                wave->nextSpawnTimer = wave->spawnIntervalTotal;
            }
            return true;
        }

        case MINIGAME_INVISI_GHOUL:
        {
            game->waveCount = 4;
            for (u8 w = 0; w < game->waveCount; w++)
            {
                LevelWave* wave = &game->waves[w];
                memset(wave, 0, sizeof(LevelWave));
                u8 count = 3 + w * 2;
                if (count > MAX_ZOMBIES_PER_WAVE) count = MAX_ZOMBIES_PER_WAVE;
                for (u8 i = 0; i < count; i++)
                {
                    ZombieType pool[] = { ZOMBIE_BASIC, ZOMBIE_CONEHEAD, ZOMBIE_NEWSPAPER };
                    wave->zombieTypes[i] = pool[rand() % 3];
                    wave->zombieRows[i] = (u8)(rand() % rowCount);
                }
                wave->zombieCount = count;
                wave->spawnedCount = 0;
                wave->spawnIntervalTotal = 200;
                wave->nextSpawnTimer = wave->spawnIntervalTotal;
            }
            return true;
        }

        case MINIGAME_LAST_STAND:
        {
            game->waveCount = 5;
            for (u8 w = 0; w < game->waveCount; w++)
            {
                LevelWave* wave = &game->waves[w];
                memset(wave, 0, sizeof(LevelWave));
                u8 count = 4 + w * 3;
                if (count > MAX_ZOMBIES_PER_WAVE) count = MAX_ZOMBIES_PER_WAVE;
                for (u8 i = 0; i < count; i++)
                {
                    ZombieType pool[] = {
                        ZOMBIE_BASIC, ZOMBIE_CONEHEAD, ZOMBIE_BUCKETHEAD,
                        ZOMBIE_FOOTBALL, ZOMBIE_SCREENDOOR
                    };
                    int poolSize = (w < 2) ? 3 : 5;
                    wave->zombieTypes[i] = pool[rand() % poolSize];
                    wave->zombieRows[i] = (u8)(rand() % rowCount);
                }
                wave->zombieCount = count;
                wave->spawnedCount = 0;
                wave->spawnIntervalTotal = 150 - w * 10;
                wave->nextSpawnTimer = wave->spawnIntervalTotal;
            }
            return true;
        }
    }
    return false;
}

f32 Minigame_GetSpeedMultiplier(const GameContext* game)
{
    if (!game) return 1.0f;
    if (Minigame_GetType(game->currentLevelIndex) == MINIGAME_NIMBLE_QUICK)
        return 2.0f;
    return 1.0f;
}

bool Minigame_CanRenderZombie(const GameContext* game)
{
    if (!game) return true;
    return Minigame_GetType(game->currentLevelIndex) != MINIGAME_INVISI_GHOUL;
}

u16 Minigame_StartingSun(const GameContext* game)
{
    if (!game) return STARTING_SUN;
    if (Minigame_GetType(game->currentLevelIndex) == MINIGAME_LAST_STAND)
        return 5000;
    return STARTING_SUN;
}

bool Minigame_SunFalls(const GameContext* game)
{
    if (!game) return true;
    return Minigame_GetType(game->currentLevelIndex) != MINIGAME_LAST_STAND;
}

void Minigame_OnPlantPlaced(GameContext* game, s8 col, s8 row, PlantType type)
{
    if (!game) return;
    MinigameType mg = Minigame_GetType(game->currentLevelIndex);

    if (mg == MINIGAME_COLUMN_LIKE)
    {
        if (type == PLANT_NONE || type >= PLANT_TYPE_COUNT)
            return;

        bool isWater = (game->worldFlags & WORLD_FLAG_HAS_WATER) != 0;
        for (s8 r = 0; r < (s8)game->rowCount; r++)
        {
            if (r == row) continue;
            GridCell* cell = &game->grid[r][col];
            if (cell->plant != PLANT_NONE) continue;
            if (isWater && (r == 2 || r == 3) && type != PLANT_LILY_PAD && type != PLANT_TANGLE_KELP && type != PLANT_SEA_SHROOM)
                continue;

            cell->plant = type;
            cell->health = Plant_MaxHealth(type);
            cell->shootTimer = 0;
        }
    }
}

void Minigame_OnPlantDestroyed(GameContext* game, PlantType type)
{
    (void)game;
    (void)type;
}

void Minigame_OnFlagCleared(GameContext* game)
{
    (void)game;
}

bool Minigame_ShouldReselect(const GameContext* game)
{
    (void)game;
    return false;
}

u16 Minigame_ExtraCost(const GameContext* game, PlantType type)
{
    (void)game;
    (void)type;
    return 0;
}
