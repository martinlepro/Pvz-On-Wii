/**
 * zombie.cpp - see zombie.h for the design notes and scope disclaimer.
 */
#include <string.h>
#include <math.h>
#include "zombie.h"
#include "game.h"
#include "minigame.h"
#include "particle.h"

/*
 * One row per ZombieType. Order must match the ZombieType enum in zombie.h.
 * health/speedMul/biteDamage are relative-scale approximations (Basic = 1.0x
 * baseline), not exact original point values -- see the scope note in
 * zombie.h.
 */
static const ZombieDef kZombieTable[ZOMBIE_TYPE_COUNT] = {
    { "basic",            "Zombie",              200,  1.00f, 12, ZFLAG_RIGGED_HUMANOID,               NULL     },
    { "conehead",         "Conehead Zombie",      560,  1.00f, 12, ZFLAG_RIGGED_HUMANOID,               "cone"   },
    { "buckhead",         "Buckethead Zombie",   1300,  1.00f, 12, ZFLAG_RIGGED_HUMANOID,               "bucket" },
    { "flag",             "Flag Zombie",           200,  1.00f, 12, ZFLAG_RIGGED_HUMANOID,               "flag"   },
    { "football",         "Football Zombie",      850,  1.60f, 16, ZFLAG_RIGGED_HUMANOID,               "helmet" },
    { "newspaper",        "Newspaper Zombie",      260,  0.90f, 12, ZFLAG_SPEEDS_UP,                     NULL     },
    { "screendoor",       "Screen Door Zombie",   500,  1.00f, 12, ZFLAG_HAS_SHIELD,                     NULL     },
    { "pole_vaulting",    "Pole Vaulting Zombie",  200,  1.90f, 12, ZFLAG_VAULTS,                        NULL     },
    { "disco",            "Disco Zombie",          300,  1.00f, 12, ZFLAG_SPAWNS_DANCERS,                NULL     },
    { "backup_dancer",    "Backup Dancer",         160,  1.00f, 10, ZFLAG_NONE,                          NULL     },
    { "snorkel",          "Snorkel Zombie",        200,  1.00f, 12, ZFLAG_SWIMS,                         NULL     },
    { "dolphin",          "Dolphin Rider Zombie",  260,  1.45f, 14, ZFLAG_VAULTS | ZFLAG_SWIMS,          NULL     },
    { "jack_in_box",      "Jack-in-the-Box Zombie",160,  1.40f, 30, ZFLAG_HEAVY_BITE | ZFLAG_EXPLODES,   NULL     },
    { "baloon",           "Balloon Zombie",        160,  1.00f, 12, ZFLAG_FLIES,                         NULL     },
    { "miner",            "Digger Zombie",         240,  1.00f, 12, ZFLAG_DIGS,                          NULL     },
    { "pogo",             "Pogo Zombie",           200,  1.00f, 12, ZFLAG_BOUNCES,                       NULL     },
    { "catapult",         "Catapult Zombie",       300,  0.80f, 12, ZFLAG_CATAPULT,                      NULL     },
    { "gargantuar",       "Gargantuar",           1800,  0.60f, 40, ZFLAG_HEAVY_BITE | ZFLAG_THROWS_IMP, NULL     },
    { "red_gargantuar",   "Giga-gargantuar",     4000,  0.60f, 50, ZFLAG_HEAVY_BITE | ZFLAG_THROWS_IMP, NULL     },
    { "giga_football",    "All-Star Zombie",      1000,  1.30f, 18, ZFLAG_NONE,                          NULL     },
    { "imp",              "Imp",                    60,  1.60f,  8, ZFLAG_NONE,                          NULL     },
    { "zombonie",         "Zomboni",               400,  1.00f, 12, ZFLAG_ICE_TRAIL,                     NULL     },
    { "yeti",             "Zombie Yeti",           800,  1.00f, 20, ZFLAG_NONE,                          NULL     },
    { "zomboss",          "Dr. Zomboss",         20000,  0.40f, 60, ZFLAG_HEAVY_BITE,                    NULL     },
    { "ladder",           "Ladder Zombie",        320,  1.00f, 12, ZFLAG_LEAVES_LADDER,                   NULL     },
    { "trash",             "Trash Can Zombie",     300,  1.00f, 12, ZFLAG_NONE,                          NULL     },
    { "cible",             "Target Zombie",        100,  1.00f, 10, ZFLAG_NONE,                          NULL     },
    { "baseball",          "Baseball Zombie",      220,  1.10f, 14, ZFLAG_NONE,                          NULL     },
    { "zombie_peashooter", "Zombie Peashooter",    220,  1.00f, 12, ZFLAG_NONE,                          NULL     },
    { "zombie_wall_nut",   "Zombie Wall-nut",      500,  0.80f, 12, ZFLAG_NONE,                          NULL     },
    { "zombie_tall_nut",   "Zombie Tall-nut",      700,  0.80f, 12, ZFLAG_NONE,                          NULL     },
    { "zombie_squash",     "Zombie Squash",        220,  1.00f, 12, ZFLAG_NONE,                          NULL     },
};

const ZombieDef* Zombie_GetDef(ZombieType type) {
    if (type < 0 || type >= ZOMBIE_TYPE_COUNT) {
        type = ZOMBIE_BASIC;
    }
    return &kZombieTable[type];
}

void Zombie_ResetAll(Zombie zombies[MAX_ZOMBIES]) {
    memset(zombies, 0, sizeof(Zombie) * MAX_ZOMBIES);
}

bool Zombie_Spawn(struct GameContext* game, ZombieType type, u8 row) {
    if (!game) return false;
    if (row >= GRID_ROWS) return false;
    ZombieRuntimeState initialState = ZSTATE_WALKING;
    if (type == ZOMBIE_BALLOON) initialState = ZSTATE_FLYING;
    if (type == ZOMBIE_MINER)   initialState = ZSTATE_DIGGING;
    if (type == ZOMBIE_SNORKEL || (type == ZOMBIE_DOLPHIN_RIDER && (row == 2 || row == 3)))
        initialState = ZSTATE_SWIMMING;

    Zombie* zombies = game->zombies;
    for (int i = 0; i < MAX_ZOMBIES; i++) {
        if (!zombies[i].active) {
            const ZombieDef* def = Zombie_GetDef(type);
            zombies[i].active = true;
            zombies[i].type = type;
            zombies[i].row = row;
            zombies[i].x = (f32)GRID_COLS + ZOMBIE_SPAWN_COL_OFFSET;
            zombies[i].health = def->health;
            zombies[i].speedMul = def->speedMul;
            zombies[i].slowFramesLeft = 0;
            zombies[i].frozenFrames = 0;
            zombies[i].state = initialState;
            zombies[i].animTimer = 0;
            zombies[i].deathTimer = 0;
            zombies[i].specialTimer = 0;
            zombies[i].specialFlag = false;
            zombies[i].movingRight = false;
            zombies[i].shieldHealth = (def->flags & ZFLAG_HAS_SHIELD) ? 500 : 0;

            /* Minigame health modifier (Big Trouble = smaller zombies) */
            if (Minigame_IsMinigame(game->currentLevelIndex) &&
                Minigame_GetType(game->currentLevelIndex) == MINIGAME_BIG_TROUBLE)
            {
                zombies[i].health = (s16)(zombies[i].health / 2);
            }
            return true;
        }
    }
    return false; /* pool full -- shouldn't happen with MAX_ZOMBIES headroom */
}

bool Zombie_NoneActive(const Zombie zombies[MAX_ZOMBIES]) {
    for (int i = 0; i < MAX_ZOMBIES; i++) {
        if (zombies[i].active) return false;
    }
    return true;
}

bool Zombie_DamageNearestInRow(Zombie zombies[MAX_ZOMBIES], u8 row, f32 minX, s16 damage, bool slows) {
    int best = -1;
    f32 bestX = 1e9f;
    for (int i = 0; i < MAX_ZOMBIES; i++) {
        Zombie* z = &zombies[i];
        if (!z->active || z->row != row || z->state == ZSTATE_DYING) continue;
        if (z->x < minX) continue;
        if (z->x < bestX) {
            bestX = z->x;
            best = i;
        }
    }
    if (best < 0) return false;

    Zombie* z = &zombies[best];

    /* Screen Door shield absorbs damage from pea-type projectiles */
    if (z->shieldHealth > 0 && !slows) {
        if (z->shieldHealth >= damage) {
            z->shieldHealth -= damage;
            return true; /* absorbed */
        }
        damage -= z->shieldHealth;
        z->shieldHealth = 0;
    }

    z->health -= damage;
    if (slows) {
        z->slowFramesLeft = SNOW_PEA_SLOW_FRAMES;
    }
    if (z->health <= 0) {
        z->state = ZSTATE_DYING;
        z->deathTimer = ZOMBIE_DEATH_FADE_FRAMES;
    }
    return true;
}

bool Zombie_AnyActiveAheadInRow(const Zombie zombies[MAX_ZOMBIES], u8 row, f32 minX) {
    for (int i = 0; i < MAX_ZOMBIES; i++) {
        const Zombie* z = &zombies[i];
        if (z->active && z->row == row && z->state != ZSTATE_DYING && z->x >= minX) {
            return true;
        }
    }
    return false;
}

void Zombie_DamageInBlastRadius(Zombie zombies[MAX_ZOMBIES], s8 col, s8 row, s16 radiusTiles, s16 damage) {
    for (int i = 0; i < MAX_ZOMBIES; i++) {
        Zombie* z = &zombies[i];
        if (!z->active || z->state == ZSTATE_DYING) continue;
        if (z->row < row - radiusTiles || z->row > row + radiusTiles) continue;
        if (z->x < (f32)(col - radiusTiles) || z->x > (f32)(col + radiusTiles + 1)) continue;

        z->health -= damage;
        if (z->health <= 0) {
            z->state = ZSTATE_DYING;
            z->deathTimer = ZOMBIE_DEATH_FADE_FRAMES;
        }
    }
}

void Zombie_DamageInRow(Zombie zombies[MAX_ZOMBIES], u8 row, s16 damage) {
    for (int i = 0; i < MAX_ZOMBIES; i++) {
        Zombie* z = &zombies[i];
        if (!z->active || z->state == ZSTATE_DYING || z->row != row) continue;
        z->health -= damage;
        if (z->health <= 0) {
            z->state = ZSTATE_DYING;
            z->deathTimer = ZOMBIE_DEATH_FADE_FRAMES;
        }
    }
}

void Zombie_FreezeAll(Zombie zombies[MAX_ZOMBIES], u32 frames) {
    for (int i = 0; i < MAX_ZOMBIES; i++) {
        Zombie* z = &zombies[i];
        if (z->active && z->state != ZSTATE_DYING) {
            z->frozenFrames = frames;
            z->slowFramesLeft = 0;
        }
    }
}

int Zombie_ClosestToCell(const Zombie zombies[MAX_ZOMBIES], s8 col, s8 row, f32 rangeTiles) {
    int best = -1;
    f32 bestDist = rangeTiles + 1.0f;
    f32 cellX = (f32)col + 0.5f;
    for (int i = 0; i < MAX_ZOMBIES; i++) {
        const Zombie* z = &zombies[i];
        if (!z->active || z->state == ZSTATE_DYING) continue;
        if (z->row < row - 1 || z->row > row + 1) continue;
        f32 dx = z->x - cellX;
        f32 dy = (f32)((s16)z->row - (s16)row);
        f32 dist = sqrtf(dx * dx + dy * dy);
        if (dist <= rangeTiles && dist < bestDist) {
            bestDist = dist;
            best = i;
        }
    }
    return best;
}

void Zombie_Hypnotize(Zombie* z) {
    if (!z || !z->active) return;
    z->state = ZSTATE_WALKING;
    z->frozenFrames = 0;
    z->health = 300;
    z->movingRight = true;
}

bool Zombie_RemoveMetal(Zombie* z) {
    if (!z || !z->active) return false;
    bool hadMetal = false;
    switch (z->type) {
        case ZOMBIE_CONEHEAD:
            z->health -= 200;
            hadMetal = true;
            break;
        case ZOMBIE_BUCKETHEAD:
            z->health -= 1100;
            hadMetal = true;
            break;
        default:
            break;
    }
    return hadMetal;
}

/* Forward decl -- avoids pulling all of game.h's grid layout into zombie.h. */
static void ZombieEatPlant(GameContext* game, Zombie* z);

bool Zombie_UpdateAll(GameContext* game) {
    bool reachedHouse = false;
    Zombie* zombies = game->zombies;

    f32 step = ZOMBIE_BASE_SPEED_TPS / (f32)TARGET_FPS;

    for (int i = 0; i < MAX_ZOMBIES; i++) {
        Zombie* z = &zombies[i];
        if (!z->active) continue;

        z->animTimer++;

        if (z->slowFramesLeft > 0) z->slowFramesLeft--;
        if (z->frozenFrames > 0) { z->frozenFrames--; continue; }

        if (z->state == ZSTATE_DYING) {
            if (z->deathTimer > 0) z->deathTimer--;
            else z->active = false;
            continue;
        }

        /* Universal: if health <= 0, die */
        if (z->health <= 0) {
            z->state = ZSTATE_DYING;
            z->deathTimer = ZOMBIE_DEATH_FADE_FRAMES;
            if (z->frozenFrames > 0)
                Particle_SpawnIce(game->particles, z->x, (f32)z->row + 0.5f);
            else if (z->type == ZOMBIE_CATAPULT || z->type == ZOMBIE_ZOMBONI)
                Particle_SpawnExplosion(game->particles, z->x, (f32)z->row + 0.5f);
            else
                Particle_SpawnPoof(game->particles, z->x, (f32)z->row + 0.5f);
            continue;
        }

        const ZombieDef* def = Zombie_GetDef(z->type);
        u32 flags = def->flags;

        f32 speed = step * z->speedMul * (z->slowFramesLeft > 0 ? SNOW_PEA_SLOW_FACTOR : 1.0f);
        speed *= Minigame_GetSpeedMultiplier(game);
        s8 col = (s8)floorf(z->x);
        bool onGrid = Game_IsValidCell(col, z->row);
        GridCell* cell = onGrid ? &game->grid[z->row][col] : NULL;
        bool plantHere = cell && cell->plant != PLANT_NONE;

        /* =================================================================
         *  FLYING (Balloon Zombie)
         * ================================================================= */
        if (flags & ZFLAG_FLIES) {
            z->state = ZSTATE_FLYING;
            z->x -= speed * 0.8f; /* slightly slower forward */
            if (z->x < 0.0f) { reachedHouse = true; z->active = false; }
            continue;
        }

        /* =================================================================
         *  DIGGING (Digger Zombie)
         * ================================================================= */
        if (flags & ZFLAG_DIGS) {
            if (z->state == ZSTATE_DIGGING) {
                /* Move left quickly underground */
                z->x -= speed * 2.0f;
                if (z->x <= 0.0f) { z->state = ZSTATE_SURFACING; z->specialTimer = TARGET_FPS / 2; }
            } else if (z->state == ZSTATE_SURFACING) {
                if (z->specialTimer > 0) z->specialTimer--;
                else z->state = ZSTATE_WALKING;
            } else {
                z->x -= speed;
                if (z->x < 0.0f) { reachedHouse = true; z->active = false; }
            }
            continue;
        }

        /* =================================================================
         *  SWIMMING (Snorkel / Dolphin Rider in water)
         * ================================================================= */
        if (flags & ZFLAG_SWIMS && (z->row == 2 || z->row == 3)) {
            z->state = ZSTATE_SWIMMING;
            z->x -= speed;
            if (z->x < 0.0f) { reachedHouse = true; z->active = false; }
            continue;
        }

        /* =================================================================
         *  EXPLODING (Jack-in-the-Box)
         * ================================================================= */
        if (flags & ZFLAG_EXPLODES) {
            if (z->specialTimer == 0)
                z->specialTimer = (u16)(TARGET_FPS * 5 + (rand() % (TARGET_FPS * 20)));
            else
                z->specialTimer--;

            if (z->specialTimer == 0) {
                /* Explode: damage nearby plants */
                Particle_SpawnExplosion(game->particles, z->x, (f32)z->row + 0.5f);
                if (onGrid) { cell->plant = PLANT_NONE; cell->health = 0; }
                for (s8 dr = -1; dr <= 1; dr++) {
                    for (s8 dc = -1; dc <= 1; dc++) {
                        s8 nr = (s8)z->row + dr;
                        s8 nc = col + dc;
                        if (Game_IsValidCell(nc, nr)) {
                            game->grid[nr][nc].plant = PLANT_NONE;
                            game->grid[nr][nc].health = 0;
                        }
                    }
                }
                z->active = false;
                continue;
            }
            /* Walk while ticking */
        }

        /* =================================================================
         *  SPAWNING DANCERS (Disco Zombie)
         * ================================================================= */
        if (flags & ZFLAG_SPAWNS_DANCERS && !z->specialFlag) {
            if (z->x <= (f32)GRID_COLS * 0.6f) {
                bool anySpawned = false;
                for (int d = 0; d < 4; d++) {
                    if (Zombie_Spawn(game, ZOMBIE_BACKUP_DANCER, z->row))
                        anySpawned = true;
                }
                if (anySpawned)
                    z->specialFlag = true;
            }
        }

        /* =================================================================
         *  VAULTING (Pole Vaulting Zombie / Dolphin Rider on land)
         * ================================================================= */
        if (flags & ZFLAG_VAULTS) {
            if (z->state == ZSTATE_VAULTING) {
                if (z->specialTimer > 0) {
                    z->specialTimer--;
                    continue; /* mid-air */
                }
                z->state = ZSTATE_WALKING;
                z->speedMul = 1.0f; /* slow down after vault */
            } else if (plantHere && !z->specialFlag) {
                /* Vault over the plant */
                z->specialFlag = true;
                z->state = ZSTATE_VAULTING;
                z->specialTimer = TARGET_FPS / 4; /* 0.25s vault animation */
                z->x = (f32)(col + 1); /* vault over the plant without destroying it */
                continue;
            }
        }

        /* =================================================================
         *  BOUNCING (Pogo Zombie)
         * ================================================================= */
        if (flags & ZFLAG_BOUNCES) {
            if (z->specialFlag) {
                /* Pogo popped: walk and eat normally */
                if (plantHere) {
                    z->state = ZSTATE_EATING;
                    if (z->animTimer % ZOMBIE_EAT_INTERVAL_FRAMES == 0)
                        ZombieEatPlant(game, z);
                } else {
                    z->state = ZSTATE_WALKING;
                    z->x -= speed;
                    if (z->x < 0.0f) { reachedHouse = true; z->active = false; }
                }
            } else if (plantHere) {
                z->state = ZSTATE_BOUNCING;
                /* Pop pogo stick on Spikeweed/Spikerock */
                if (cell && (cell->plant == PLANT_SPIKEWEED || cell->plant == PLANT_SPIKEROCK)) {
                    z->speedMul = def->speedMul; /* lose the bounce speed bonus */
                    z->specialFlag = true; /* pogo popped */
                } else {
                    s8 bounceCol = col + 1;
                    if (bounceCol > GRID_COLS) bounceCol = GRID_COLS;
                    z->x = (f32)bounceCol; /* bounce over */
                }
            } else {
                z->state = ZSTATE_WALKING;
                z->x -= speed * 1.4f;
                if (z->x < 0.0f) { reachedHouse = true; z->active = false; }
            }
            continue;
        }

        /* =================================================================
         *  CATAPULT (Catapult Zombie)
         * ================================================================= */
        if (flags & ZFLAG_CATAPULT) {
            if (z->x <= (f32)GRID_COLS * 0.7f && z->specialTimer == 0) {
                z->state = ZSTATE_THROWING;
                z->specialTimer = TARGET_FPS * 2; /* throw interval */
                /* Damage a random plant in its row */
                for (s8 c = GRID_COLS - 1; c >= 0; c--) {
                    GridCell* t = &game->grid[z->row][c];
                    if (t->plant != PLANT_NONE) {
                        t->health -= 40;
                        if (t->health <= 0) { t->plant = PLANT_NONE; t->health = 0; }
                        break;
                    }
                }
            }
            if (z->specialTimer > 0) z->specialTimer--;
        }

        /* =================================================================
         *  GARGANTUAR: throw Imp at half health
         * ================================================================= */
        if (flags & ZFLAG_THROWS_IMP && !z->specialFlag && z->health <= def->health / 2) {
            if (Zombie_Spawn(game, ZOMBIE_IMP, z->row))
                z->specialFlag = true;
        }

        /* =================================================================
         *  ICE TRAIL (Zomboni)
         * ================================================================= */
        if (flags & ZFLAG_ICE_TRAIL && onGrid) {
            if (cell->plant != PLANT_NONE) {
                cell->health = 0;
                cell->plant = PLANT_NONE;
            }
        }

        /* =================================================================
         *  SHIELD (Screen Door) — projectile block handled in combat.cpp
         * ================================================================= */

        /* =================================================================
         *  LADDER: places ladder over first plant encountered, walks over
         * ================================================================= */
        if (flags & ZFLAG_LEAVES_LADDER && plantHere && !cell->hasLadder && !z->specialFlag) {
            cell->hasLadder = true;
            z->specialFlag = true;
            /* Continue walking without eating */
        }

        /* =================================================================
         *  STANDARD WALK / EAT
         *  If cell has a ladder, zombies walk over the plant without eating.
         * ================================================================= */
        if (plantHere && !cell->hasLadder) {
            z->state = ZSTATE_EATING;
            if (z->animTimer % ZOMBIE_EAT_INTERVAL_FRAMES == 0)
                ZombieEatPlant(game, z);
        } else {
            z->state = ZSTATE_WALKING;
            if (z->movingRight)
                z->x += speed;
            else
                z->x -= speed;
            if (z->x < 0.0f || z->x > (f32)(GRID_COLS + ZOMBIE_SPAWN_COL_OFFSET + 2))
                { reachedHouse = true; z->active = false; }
        }
    }
    return reachedHouse;
}

static void ZombieEatPlant(GameContext* game, Zombie* z) {
    const ZombieDef* def = Zombie_GetDef(z->type);
    s8 col = (s8)floorf(z->x);
    if (!Game_IsValidCell(col, z->row)) return;

    GridCell* cell = &game->grid[z->row][col];
    if (cell->plant == PLANT_NONE) return;

    u16 dmg = def->biteDamage;

    /* Pumpkin armor: damage pumpkin first */
    if (cell->hasPumpkin) {
        if (cell->pumpkinHealth <= (s16)dmg) {
            cell->hasPumpkin = false;
            cell->pumpkinHealth = 0;
        } else {
            cell->pumpkinHealth -= (s16)dmg;
        }
        return;
    }

    /* Garlic diversion: when eaten, zombie moves to adjacent row */
    if (cell->plant == PLANT_GARLIC) {
        cell->health = 0;
        cell->plant = PLANT_NONE;
        s8 newRow = z->row + ((rand() % 2) ? 1 : -1);
        if (newRow < 0) newRow = 0;
        if (newRow >= (s8)game->rowCount) newRow = (s8)(game->rowCount - 1);
        z->row = (u8)newRow;
        return;
    }

    if (cell->health <= (s16)dmg) {
        cell->health = 0;
        cell->plant = PLANT_NONE;
        cell->shootTimer = 0;
    } else {
        cell->health -= (s16)dmg;
    }

    if (def->flags & ZFLAG_SPEEDS_UP) {
        z->speedMul = def->speedMul * 1.7f;
    }
}
