#include <string.h>
#include <math.h>
#include "combat.h"
#include "game.h"
#include "particle.h"
#include "zombie.h"

/* -------------------------------------------------------------------------- */
/* Helpers                                                                    */
/* -------------------------------------------------------------------------- */

static bool IsShooter(PlantType t) {
    switch (t) {
        case PLANT_PEASHOOTER:
        case PLANT_SNOW_PEA:
        case PLANT_REPEATER:
        case PLANT_THREEPEATER:
        case PLANT_SPLIT_PEA:
        case PLANT_STARFRUIT:
        case PLANT_GATLING_PEA:
        case PLANT_PUFF_SHROOM:
        case PLANT_FUME_SHROOM:
        case PLANT_SEA_SHROOM:
        case PLANT_SCAREDY_SHROOM:
        case PLANT_CACTUS:
        case PLANT_CABBAGE_PULT:
        case PLANT_KERNEL_PULT:
        case PLANT_MELON_PULT:
        case PLANT_WINTER_MELON:
        case PLANT_GLOOM_SHROOM:
        case PLANT_CATTAIL:
            return true;
        default:
            return false;
    }
}

static bool IsAquatic(PlantType t) {
    return t == PLANT_SEA_SHROOM || t == PLANT_TANGLE_KELP || t == PLANT_LILY_PAD;
}

/* Number of projectiles per trigger for multi-shot plants. */
static int ShotCount(PlantType t) {
    if (t == PLANT_REPEATER || t == PLANT_SPLIT_PEA) return 2;
    if (t == PLANT_GATLING_PEA) return 4;
    if (t == PLANT_GLOOM_SHROOM) return 8;
    if (t == PLANT_CATTAIL) return 2;
    if (t == PLANT_STARFRUIT) return 5;
    return 1;
}

/* Cooldown frames between shots. */
static u16 Cooldown(PlantType t) {
    switch (t) {
        case PLANT_GATLING_PEA:     return (u16)GATLING_COOLDOWN_FRAMES;
        case PLANT_REPEATER:        return (u16)REPEATER_COOLDOWN_FRAMES;
        case PLANT_CABBAGE_PULT:
        case PLANT_KERNEL_PULT:
        case PLANT_MELON_PULT:
        case PLANT_WINTER_MELON:    return (u16)LOB_COOLDOWN_FRAMES;
        case PLANT_PUFF_SHROOM:
        case PLANT_SEA_SHROOM:
        case PLANT_FUME_SHROOM:     return (u16)FUME_COOLDOWN_FRAMES;
        case PLANT_GLOOM_SHROOM:    return (u16)GLOOM_COOLDOWN_FRAMES;
        default:                    return (u16)SHOOTER_COOLDOWN_FRAMES;
    }
}

/* Projectile range in tiles. */
static f32 RangeTiles(PlantType t) {
    switch (t) {
        case PLANT_PUFF_SHROOM:
        case PLANT_SEA_SHROOM:      return SPORE_RANGE_TILES;
        case PLANT_GLOOM_SHROOM:    return 1.5f;
        default:                    return PEA_RANGE_TILES;
    }
}

/* Projectile speed in tiles/sec. */
static f32 ProjSpeed(ProjectileKind kind) {
    (void)kind;
    return PEA_SPEED_TPS;
}

/* -------------------------------------------------------------------------- */
/* Spawning a projectile into the pool                                       */
/* -------------------------------------------------------------------------- */

static int SpawnProj(GameContext* game, u8 row, f32 x, f32 y, s16 damage,
                     ProjectileKind kind, bool slows, u8 splashRadius)
{
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        Projectile* p = &game->projectiles[i];
        if (p->active) continue;
        p->active = true;
        p->row = row;
        p->x = x;
        p->y = y;
        p->dx = 1.0f; /* default rightward */
        p->dy = 0.0f;
        p->damage = damage;
        p->kind = kind;
        p->slows = slows;
        p->pierces = (kind == PROJ_FUME);
        p->splashRadius = splashRadius;
        p->life = 255;
        return i;
    }
    return -1;
}

/* -------------------------------------------------------------------------- */
/* Shooter-plant tick: decide whether to fire, and spawn the projectile(s).   */
/* Returns true if at least one projectile was spawned.                       */
/* -------------------------------------------------------------------------- */

static bool FireShooter(GameContext* game, s8 row, s8 col, PlantType type)
{
    u8 urow = (u8)row;
    f32 startX = (f32)col + 0.6f;
    f32 cellCY = (f32)row + 0.5f;
    s16 dmg = (type == PLANT_CABBAGE_PULT || type == PLANT_MELON_PULT ||
               type == PLANT_WINTER_MELON) ? 40 : 20;
    if (type == PLANT_MELON_PULT || type == PLANT_WINTER_MELON) dmg = 80;

    switch (type) {
        /* ---- Straight shooters ---- */
        case PLANT_PEASHOOTER:
        case PLANT_SNOW_PEA: {
            ProjectileKind k = (type == PLANT_SNOW_PEA) ? PROJ_SNOW : PROJ_PEA;
            SpawnProj(game, urow, startX, cellCY, PEA_DAMAGE, k,
                      (type == PLANT_SNOW_PEA), 0);
            return true;
        }

        case PLANT_REPEATER:
            SpawnProj(game, urow, startX, cellCY, PEA_DAMAGE, PROJ_PEA, false, 0);
            SpawnProj(game, urow, startX + 0.15f, cellCY, PEA_DAMAGE, PROJ_PEA, false, 0);
            return true;

        case PLANT_GATLING_PEA:
            for (int s = 0; s < 4; s++)
                SpawnProj(game, urow, startX + s * 0.1f, cellCY, PEA_DAMAGE, PROJ_PEA, false, 0);
            return true;

        case PLANT_THREEPEATER: {
            for (s8 r = row - 1; r <= row + 1; r++) {
                if (r < 0 || r >= (s8)game->rowCount) continue;
                SpawnProj(game, (u8)r, startX, (f32)r + 0.5f, PEA_DAMAGE, PROJ_PEA, false, 0);
            }
            return true;
        }

        case PLANT_SPLIT_PEA:
            /* forward */
            SpawnProj(game, urow, startX, cellCY, PEA_DAMAGE, PROJ_PEA, false, 0);
            /* backward (two peas, dx = -1) */
            for (int s = 0; s < 2; s++) {
                int idx = SpawnProj(game, urow, (f32)col - 0.1f, cellCY, PEA_DAMAGE, PROJ_PEA, false, 0);
                if (idx >= 0) {
                    game->projectiles[idx].dx = -1.0f;
                    game->projectiles[idx].x = (f32)col - 0.1f - s * 0.1f;
                }
            }
            return true;

        case PLANT_STARFRUIT: {
            /* 5 directions: 0, 72, 144, 216, 288 degrees */
            f32 angles[5] = { 0.0f, 1.2566f, 2.5133f, 3.7699f, 5.0265f };
            for (int s = 0; s < 5; s++) {
                int idx = SpawnProj(game, urow, startX, cellCY, PEA_DAMAGE, PROJ_STAR, false, 0);
                if (idx >= 0) {
                    game->projectiles[idx].dx = cosf(angles[s]);
                    game->projectiles[idx].dy = sinf(angles[s]);
                }
            }
            return true;
        }

        /* ---- Short-range / piercing ---- */
        case PLANT_PUFF_SHROOM:
        case PLANT_SEA_SHROOM: {
            ProjectileKind k = PROJ_SPORE;
            int idx = SpawnProj(game, urow, startX, cellCY, PEA_DAMAGE, k, false, 0);
            if (idx >= 0) game->projectiles[idx].life = (u8)(SPORE_RANGE_TILES * 10);
            return true;
        }

        case PLANT_FUME_SHROOM: {
            int idx = SpawnProj(game, urow, startX, cellCY, PEA_DAMAGE, PROJ_FUME, false, 0);
            if (idx >= 0) game->projectiles[idx].life = (u8)(FUME_RANGE_TILES * 10);
            return true;
        }

        case PLANT_SCAREDY_SHROOM: {
            /* Hide if zombie is within 4 tiles AHEAD; otherwise fire */
            bool scared = false;
            for (int zi = 0; zi < MAX_ZOMBIES; zi++) {
                const Zombie* z = &game->zombies[zi];
                if (!z->active || z->state == ZSTATE_DYING || z->row != urow) continue;
                f32 dx = z->x - (f32)col;
                if (dx >= 0 && dx <= 4.0f) { scared = true; break; }
            }
            if (!scared) {
                SpawnProj(game, urow, startX, cellCY, PEA_DAMAGE, PROJ_PEA, false, 0);
                return true;
            }
            return false;
        }

        /* ---- Lobbed ---- */
        case PLANT_CABBAGE_PULT:
            SpawnProj(game, urow, startX, cellCY, dmg, PROJ_CABBAGE, false, 0);
            return true;

        case PLANT_KERNEL_PULT: {
            /* 30% chance butter */
            ProjectileKind k = (rand() % 10 < 3) ? PROJ_BUTTER : PROJ_KERNEL;
            s16 kdmg = (k == PROJ_BUTTER) ? 40 : 20;
            SpawnProj(game, urow, startX, cellCY, kdmg, k, false, 0);
            return true;
        }

        case PLANT_MELON_PULT:
            SpawnProj(game, urow, startX, cellCY, dmg, PROJ_MELON, false, 1);
            return true;

        case PLANT_WINTER_MELON:
            SpawnProj(game, urow, startX, cellCY, dmg, PROJ_WINTER, true, 1);
            return true;

        /* ---- Special ---- */
        case PLANT_GLOOM_SHROOM: {
            /* 8-directional short-range spray */
            f32 angles[8] = { 0, 0.785f, 1.571f, 2.356f, 3.142f, 3.927f, 4.712f, 5.498f };
            for (int s = 0; s < 8; s++) {
                int idx = SpawnProj(game, urow, startX, cellCY, 20, PROJ_FUME, false, 0);
                if (idx >= 0) {
                    game->projectiles[idx].dx = cosf(angles[s]) * 0.5f;
                    game->projectiles[idx].dy = sinf(angles[s]) * 0.5f;
                    game->projectiles[idx].life = 15; /* ~1.5 tiles */
                    game->projectiles[idx].pierces = false;
                }
            }
            return true;
        }

        case PLANT_CACTUS:
            SpawnProj(game, urow, startX, cellCY, PEA_DAMAGE, PROJ_SPIKE, false, 0);
            return true;

        case PLANT_CATTAIL: {
            /* Fire homing spikes at nearest zombie in any row */
            int best = -1;
            f32 bestDist = 1e9f;
            for (int zi = 0; zi < MAX_ZOMBIES; zi++) {
                const Zombie* z = &game->zombies[zi];
                if (!z->active || z->state == ZSTATE_DYING) continue;
                f32 dz = z->x - (f32)col;
                if (dz < 0) continue; /* only target zombies ahead */
                f32 dist = sqrtf(dz * dz + (f32)((s16)z->row - (s16)row) * (f32)((s16)z->row - (s16)row));
                if (dist < bestDist) {
                    bestDist = dist;
                    best = zi;
                }
            }
            for (int s = 0; s < 2; s++) {
                int idx = SpawnProj(game, urow, startX, cellCY, PEA_DAMAGE, PROJ_HOMING, false, 0);
                if (idx >= 0 && best >= 0) {
                    Projectile* p = &game->projectiles[idx];
                    const Zombie* target = &game->zombies[best];
                    f32 dx = target->x - p->x;
                    f32 dy = (f32)((s16)target->row - (s16)row);
                    f32 len = sqrtf(dx * dx + dy * dy);
                    if (len > 0.01f) { p->dx = dx / len; p->dy = dy / len; }
                }
            }
            return true;
        }

        default:
            return false;
    }
}

/* -------------------------------------------------------------------------- */
/* Instant-use plant triggers (checked once when a zombie enters the cell)    */
/* -------------------------------------------------------------------------- */

static void CheckInstants(GameContext* game) {
    for (s8 row = 0; row < (s8)game->rowCount; row++) {
        for (s8 col = 0; col < GRID_COLS; col++) {
            GridCell* cell = &game->grid[row][col];
            if (cell->plant == PLANT_NONE) continue;

            switch (cell->plant) {
                case PLANT_CHERRY_BOMB:
                    Particle_SpawnExplosion(game->particles, (f32)col + 0.5f, (f32)row + 0.5f);
                    cell->plant = PLANT_NONE;
                    cell->health = 0;
                    break;

                case PLANT_POTATO_MINE: {
                    if (cell->shootTimer < 900) {
                        cell->shootTimer++;
                        break;
                    }
                    int zi = Zombie_ClosestToCell(game->zombies, col, row, 1.2f);
                    if (zi >= 0) {
                        Zombie_DamageInBlastRadius(game->zombies, col, row, 1, 1800);
                        Particle_SpawnExplosion(game->particles, (f32)col + 0.5f, (f32)row + 0.5f);
                        cell->plant = PLANT_NONE;
                        cell->health = 0;
                    }
                    break;
                }

                case PLANT_SQUASH: {
                    int zi = Zombie_ClosestToCell(game->zombies, col, row, 1.2f);
                    if (zi >= 0) {
                        Zombie_DamageInBlastRadius(game->zombies, col, row, 0, 1800);
                        Particle_SpawnPoof(game->particles, (f32)col + 0.5f, (f32)row + 0.5f);
                        cell->plant = PLANT_NONE;
                        cell->health = 0;
                    }
                    break;
                }

                case PLANT_CHOMPER: {
                    int zi = Zombie_ClosestToCell(game->zombies, col, row, 1.5f);
                    if (zi >= 0 && cell->shootTimer == 0) {
                        Zombie* target = &game->zombies[zi];
                        if (target->health <= 400) {
                            target->state = ZSTATE_DYING;
                            target->deathTimer = ZOMBIE_DEATH_FADE_FRAMES;
                            cell->shootTimer = 42 * TARGET_FPS / 60; /* ~42s chewing */
                        } else {
                            target->health -= 40;
                            cell->shootTimer = TARGET_FPS; /* bite cooldown */
                        }
                    }
                    if (cell->shootTimer > 0) cell->shootTimer--;
                    break;
                }

                case PLANT_TANGLE_KELP: {
                    int zi = Zombie_ClosestToCell(game->zombies, col, row, 1.2f);
                    if (zi >= 0) {
                        Zombie_DamageInBlastRadius(game->zombies, col, row, 0, 1800);
                        cell->plant = PLANT_NONE;
                        cell->health = 0;
                    }
                    break;
                }

                case PLANT_JALAPENO:
                    Zombie_DamageInRow(game->zombies, (u8)row, JALAPENO_DAMAGE);
                    Particle_SpawnExplosion(game->particles, (f32)col + 0.5f, (f32)row + 0.5f);
                    cell->plant = PLANT_NONE;
                    cell->health = 0;
                    break;

                case PLANT_ICE_SHROOM:
                    Zombie_FreezeAll(game->zombies, ICE_FREEZE_FRAMES);
                    Particle_SpawnIce(game->particles, (f32)col + 0.5f, (f32)row + 0.5f);
                    cell->plant = PLANT_NONE;
                    cell->health = 0;
                    break;

                case PLANT_DOOM_SHROOM:
                    Zombie_DamageInBlastRadius(game->zombies, col, row, DOOM_SHROOM_RADIUS_TILES, 1800);
                    Particle_SpawnExplosion(game->particles, (f32)col + 0.5f, (f32)row + 0.5f);
                    cell->plant = PLANT_NONE;
                    cell->health = 0;
                    break;

                case PLANT_HYPNO_SHROOM: {
                    int zi = Zombie_ClosestToCell(game->zombies, col, row, 1.0f);
                    if (zi >= 0) {
                        Zombie_Hypnotize(&game->zombies[zi]);
                        cell->plant = PLANT_NONE;
                        cell->health = 0;
                    }
                    break;
                }

                case PLANT_MAGNET_SHROOM: {
                    /* Remove metal from nearest zombie ahead in the row */
                    for (int zi = 0; zi < MAX_ZOMBIES; zi++) {
                        Zombie* z = &game->zombies[zi];
                        if (!z->active || z->state == ZSTATE_DYING) continue;
                        if (z->row != (u8)row) continue;
                        if (z->x < (f32)col) continue;
                        if (Zombie_RemoveMetal(z)) {
                            cell->shootTimer = TARGET_FPS * 15; /* 15s cooldown */
                            break;
                        }
                    }
                    if (cell->shootTimer > 0) cell->shootTimer--;
                    break;
                }

                case PLANT_COFFEE_BEAN: {
                    /* Wake sleeping mushrooms on this cell */
                    /* Coffee Bean is placed on top of an existing shroom,
                     * but we handle it by checking if a mushroom is here
                     * and activating it. Since mushrooms work by default
                     * in our system (no sleep state), this is a no-op
                     * for now. Future: add asleep/awake state. */
                    cell->plant = PLANT_NONE;
                    cell->health = 0;
                    break;
                }

                case PLANT_BLOVER: {
                    /* Blow away all balloon zombies + fog */
                    for (int zi = 0; zi < MAX_ZOMBIES; zi++) {
                        Zombie* z = &game->zombies[zi];
                        if (!z->active || z->state == ZSTATE_DYING) continue;
                        if (z->type == ZOMBIE_BALLOON) {
                            z->state = ZSTATE_DYING;
                            z->deathTimer = ZOMBIE_DEATH_FADE_FRAMES;
                        }
                    }
                    cell->plant = PLANT_NONE;
                    cell->health = 0;
                    break;
                }

                default:
                    break;
            }
        }
    }
}

/* -------------------------------------------------------------------------- */
/* Torchwood: upgrade PEA to FIRE if it passes through a Torchwood cell       */
/* -------------------------------------------------------------------------- */

static void ApplyTorchwood(GameContext* game, Projectile* p) {
    if (p->kind != PROJ_PEA && p->kind != PROJ_SNOW)
        return;
    s8 col = (s8)floorf(p->x);
    s8 row = (s8)p->row;
    if (col < 0 || col >= GRID_COLS || row < 0 || row >= (s8)game->rowCount)
        return;
    if (game->grid[row][col].plant == PLANT_TORCHWOOD) {
        p->kind = PROJ_FIRE;
        p->damage = (s16)((s16)p->damage * 2);
        p->slows = false; /* fire overrides snow */
    }
}

/* -------------------------------------------------------------------------- */
/* Projectile advance + collision                                             */
/* -------------------------------------------------------------------------- */

static void AdvanceProjectiles(GameContext* game) {
    f32 step = PEA_SPEED_TPS / (f32)TARGET_FPS;
    for (int i = 0; i < MAX_PROJECTILES; i++) {
        Projectile* p = &game->projectiles[i];
        if (!p->active) continue;

        p->x += p->dx * step;
        p->y += p->dy * step;
        if (p->life > 0) p->life--;

        /* Out of bounds */
        if (p->x > (f32)GRID_COLS + 2.0f || p->x < -2.0f ||
            p->y < -2.0f || p->y > (f32)GRID_ROWS + 2.0f ||
            p->life == 0)
        {
            p->active = false;
            continue;
        }

        /* Torchwood check (before collision) */
        if (p->kind == PROJ_PEA || p->kind == PROJ_SNOW)
            ApplyTorchwood(game, p);

        /* Determine effective row for collision */
        u8 collRow = p->row;
        if (p->kind == PROJ_STAR || p->kind == PROJ_HOMING) {
            s8 py = (s8)floorf(p->y + 0.5f);
            if (py >= 0 && py < (s8)game->rowCount)
                collRow = (u8)py;
            else {
                p->active = false;
                continue;
            }
        }

        /* Collision: find the nearest active zombie at or ahead of the projectile,
         * respecting immunity rules (Balloon → only PROJ_SPIKE, Snorkel swimming →
         * immune to peas, Digger → immune while dug, Screen Door → shield absorbed). */
        int hitIdx = -1;
        f32 hitX = 1e9f;
        for (int zi = 0; zi < MAX_ZOMBIES; zi++) {
            Zombie* z = &game->zombies[zi];
            if (!z->active || z->state == ZSTATE_DYING || z->row != collRow) continue;
            if (z->x < p->x) continue;

            /* Immunity checks */
            if (z->state == ZSTATE_FLYING) {
                if (p->kind == PROJ_SPIKE) { /* Cactus can pop Balloon */ }
                else continue;
            }
            if (z->state == ZSTATE_SWIMMING) {
                if (p->kind == PROJ_SPORE || p->kind == PROJ_FUME || p->kind == PROJ_CABBAGE ||
                    p->kind == PROJ_KERNEL || p->kind == PROJ_MELON || p->kind == PROJ_WINTER)
                    { /* aquatic / lobbed can hit */ }
                else continue;
            }
            if (z->state == ZSTATE_DIGGING || z->state == ZSTATE_SURFACING) continue;

            if (z->x < hitX) { hitX = z->x; hitIdx = zi; }
        }

        if (hitIdx >= 0) {
            Zombie* z = &game->zombies[hitIdx];
            s16 dmg = p->damage;

            /* Screen Door shield blocks pea-type damage unless it's fume/lobbed */
            if (z->shieldHealth > 0 && p->kind != PROJ_FUME && p->kind != PROJ_CABBAGE &&
                p->kind != PROJ_KERNEL && p->kind != PROJ_MELON && p->kind != PROJ_WINTER)
            {
                if (z->shieldHealth >= dmg) { z->shieldHealth -= dmg; dmg = 0; }
                else { dmg -= z->shieldHealth; z->shieldHealth = 0; }
            }

            if (dmg > 0) {
                z->health -= dmg;
                if (p->slows) z->slowFramesLeft = SNOW_PEA_SLOW_FRAMES;
                if (z->health <= 0) {
                    z->state = ZSTATE_DYING;
                    z->deathTimer = ZOMBIE_DEATH_FADE_FRAMES;
                }
                f32 py = (f32)collRow + 0.5f;
                if (p->kind == PROJ_SNOW || p->kind == PROJ_WINTER)
                    Particle_SpawnIce(game->particles, p->x, py);
                else if (p->kind == PROJ_BUTTER)
                    Particle_SpawnButter(game->particles, p->x, py);
                else
                    Particle_Spawn(game->particles, PARTICLE_SPARK, p->x, py, 3);
            }

            /* Butter stun */
            if (p->kind == PROJ_BUTTER) {
                z->slowFramesLeft = BUTTER_STUN_FRAMES;
            }

            /* Projectile consumed */
            if (p->pierces) {
                p->damage = (s16)((s16)p->damage / 2);
                if (p->damage <= 0 || dmg <= 0) p->active = false;
            } else {
                p->active = false;
            }
        }
    }
}

/* -------------------------------------------------------------------------- */
/* Spikeweed / Spikerock: damage zombies standing on them                     */
/* -------------------------------------------------------------------------- */

static void CheckSpikeweed(GameContext* game) {
    for (s8 row = 0; row < (s8)game->rowCount; row++) {
        for (s8 col = 0; col < GRID_COLS; col++) {
            GridCell* cell = &game->grid[row][col];
            if (cell->plant != PLANT_SPIKEWEED && cell->plant != PLANT_SPIKEROCK)
                continue;
            u16 dmg = (cell->plant == PLANT_SPIKEROCK) ? 40 : 20;
            /* Only damage zombies that are within this cell */
            for (int zi = 0; zi < MAX_ZOMBIES; zi++) {
                Zombie* z = &game->zombies[zi];
                if (!z->active || z->state == ZSTATE_DYING) continue;
                if (z->row != (u8)row) continue;
                s8 zcol = (s8)floorf(z->x);
                if (zcol == col) {
                    if (cell->shootTimer >= TARGET_FPS)
                        break; /* already damaged this second */
                    z->health -= (s16)dmg;
                    if (z->health <= 0) {
                        z->state = ZSTATE_DYING;
                        z->deathTimer = ZOMBIE_DEATH_FADE_FRAMES;
                    }
                    cell->shootTimer = TARGET_FPS;
                    break; /* only damage one zombie per frame */
                }
            }
        }
    }
}

/* -------------------------------------------------------------------------- */
/* Public API                                                                 */
/* -------------------------------------------------------------------------- */

void Combat_ResetAll(GameContext* game) {
    memset(game->projectiles, 0, sizeof(game->projectiles));
}

void Combat_Update(GameContext* game) {
    if (!game || game->state != GAME_STATE_PLAYING)
        return;

    /* 1) Shooter plants: tick cooldowns, fire when a zombie is ahead */
    for (s8 row = 0; row < (s8)game->rowCount; row++) {
        for (s8 col = 0; col < GRID_COLS; col++) {
            GridCell* cell = &game->grid[row][col];
            if (!IsShooter(cell->plant)) continue;

            /* Gloom-shroom: special fast initial fire rate */
            if (cell->plant == PLANT_GLOOM_SHROOM && cell->shootTimer <= GLOOM_INITIAL_COOLDOWN_FRAMES) {
                cell->shootTimer++;
            }

            if (cell->shootTimer > 0) {
                cell->shootTimer--;
                continue;
            }
            if (!Zombie_AnyActiveAheadInRow(game->zombies, (u8)row, (f32)col - 0.5f))
                continue;

            if (FireShooter(game, row, col, cell->plant)) {
                cell->shootTimer = Cooldown(cell->plant);
            }
        }
    }

    /* 2) Instant-use plant triggers */
    CheckInstants(game);

    /* 3) Spikeweed/Spikerock */
    CheckSpikeweed(game);

    /* 4) Projectile movement + collision */
    AdvanceProjectiles(game);
}

void Combat_TriggerCherryBomb(GameContext* game, s8 col, s8 row) {
    if (!game) return;
    Zombie_DamageInBlastRadius(game->zombies, col, row, CHERRY_BOMB_RADIUS_TILES, CHERRY_BOMB_DAMAGE);
}
