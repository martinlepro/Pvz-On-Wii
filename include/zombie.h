/**
 * zombie.h - Zombie types, stats, and per-instance walking/eating/dying state.
 *
 * Art comes from assets/zombies/<slug>/full.png (a single background-removed
 * cutout per type) except the five "rigged" humanoid types (Basic, Conehead,
 * Buckethead, Flag, Football), which share one body rig sliced into parts
 * -- assets/zombies/basic/parts/{head,torso,arm,leg_a,leg_b}.png -- plus an
 * accessory (cone/bucket/flag/helmet) drawn on the head. Render_DrawZombie()
 * (render.cpp) composites the rigged ones frame-by-frame; the rest are drawn
 * as a single texture with a procedural bob/sway/lean, since the source art
 * is one static pose per type rather than a hand-drawn walk-cycle strip.
 * Either way, the animation itself -- the position/rotation math -- runs on
 * the Wii every frame; nothing is pre-baked.
 *
 * SCOPE NOTE: stat numbers are reasonable relative approximations, not exact
 * original-game values. A handful of iconic special behaviors are
 * implemented (see ZombieFlags below); most zombies' unique original
 * mechanics (pogo-jump, digger tunneling, balloon flight + Cactus counter,
 * catapult ranged attacks, Gargantuar's Imp throw, Zomboni's ice trail,
 * Dr. Zomboss's actual boss fight, etc.) are NOT implemented yet -- those
 * types currently just walk, eat, and take damage like a reskinned Basic
 * Zombie. Follow-up work, not a claim of full fidelity.
 */
#ifndef PVZ_ZOMBIE_H
#define PVZ_ZOMBIE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "constants.h"
#include "types.h"

struct GameContext; /* defined in game.h; forward-declared to avoid a circular include */

typedef enum {
    ZOMBIE_BASIC = 0,
    ZOMBIE_CONEHEAD,
    ZOMBIE_BUCKETHEAD,
    ZOMBIE_FLAG,
    ZOMBIE_FOOTBALL,
    ZOMBIE_NEWSPAPER,
    ZOMBIE_SCREENDOOR,
    ZOMBIE_POLE_VAULTING,
    ZOMBIE_DISCO,
    ZOMBIE_BACKUP_DANCER,
    ZOMBIE_SNORKEL,
    ZOMBIE_DOLPHIN_RIDER,
    ZOMBIE_JACK_IN_BOX,
    ZOMBIE_BALLOON,
    ZOMBIE_MINER,
    ZOMBIE_POGO,
    ZOMBIE_CATAPULT,
    ZOMBIE_GARGANTUAR,
    ZOMBIE_RED_GARGANTUAR,
    ZOMBIE_GIGA_FOOTBALL,
    ZOMBIE_IMP,
    ZOMBIE_ZOMBONI,
    ZOMBIE_YETI,
    ZOMBIE_ZOMBOSS,
    ZOMBIE_LADDER,
    ZOMBIE_TRASH,
    ZOMBIE_CIBLE,
    ZOMBIE_BASEBALL,
    ZOMBIE_ZOMBIE_PEASHOOTER,
    ZOMBIE_ZOMBIE_WALLNUT,
    ZOMBIE_ZOMBIE_TALLNUT,
    ZOMBIE_ZOMBIE_SQUASH,
    ZOMBIE_TYPE_COUNT
} ZombieType;

typedef enum {
    ZFLAG_NONE           = 0,
    ZFLAG_RIGGED_HUMANOID = 1 << 0,
    ZFLAG_SPEEDS_UP      = 1 << 1,
    ZFLAG_HEAVY_BITE     = 1 << 2,
    ZFLAG_FLIES          = 1 << 3,  /* Balloon: immune to ground projectiles */
    ZFLAG_SWIMS          = 1 << 4,  /* Snorkel: underwater, immune to peas   */
    ZFLAG_DIGS           = 1 << 5,  /* Digger: travels underground           */
    ZFLAG_VAULTS         = 1 << 6,  /* Pole Vault / Dolphin Rider: jumps over first plant */
    ZFLAG_BOUNCES        = 1 << 7,  /* Pogo: bounces over plants             */
    ZFLAG_HAS_SHIELD     = 1 << 8,  /* Screen Door: blocks front peas        */
    ZFLAG_EXPLODES       = 1 << 9,  /* Jack-in-the-Box: random explosion     */
    ZFLAG_CATAPULT       = 1 << 10, /* Catapult: throws from range           */
    ZFLAG_SPAWNS_DANCERS = 1 << 11, /* Disco: spawns Backup Dancers          */
    ZFLAG_THROWS_IMP     = 1 << 12, /* Gargantuar: throws Imp at half HP     */
    ZFLAG_ICE_TRAIL      = 1 << 13, /* Zomboni: leaves ice trail             */
    ZFLAG_LEAVES_LADDER  = 1 << 14, /* Ladder: places ladder over plant      */
} ZombieFlags;

/** One row of the zombie stat/art table -- see ZombieTable in zombie.cpp. */
typedef struct {
    const char* assetSlug;     /* assets/zombies/<assetSlug>/full.png            */
    const char* displayName;
    s16   health;
    f32   speedMul;            /* multiplies ZOMBIE_BASE_SPEED_TPS               */
    u16   biteDamage;          /* damage per bite to whatever plant it's eating   */
    u32   flags;
    const char* accessorySlug; /* NULL, or "cone"/"bucket"/"flag"/"helmet" (rigged only) */
} ZombieDef;

typedef enum {
    ZSTATE_WALKING = 0,
    ZSTATE_EATING,
    ZSTATE_DYING,
    ZSTATE_FLYING,      /* Balloon: airborne, ignored by ground plants */
    ZSTATE_DIGGING,     /* Digger: underground, invisible              */
    ZSTATE_SURFACING,   /* Digger: rising from underground             */
    ZSTATE_SWIMMING,    /* Snorkel: underwater in pool lanes           */
    ZSTATE_VAULTING,    /* Pole Vault / Dolphin: jumping over plant    */
    ZSTATE_BOUNCING,    /* Pogo: bouncing over plants                  */
    ZSTATE_THROWING,    /* Catapult: ranged attack                     */
} ZombieRuntimeState;

typedef struct {
    bool        active;
    ZombieType  type;
    u8          row;
    f32         x;
    s16         health;
    f32         speedMul;
    u32         slowFramesLeft;
    u32         frozenFrames;
    ZombieRuntimeState state;
    u32         animTimer;
    u32         deathTimer;

    /* Special behaviour state */
    u16         specialTimer;   /* generic countdown (vault anim, dig delay, explosion, etc.) */
    bool        specialFlag;    /* generic one-shot flag (vaulted, shield broken, etc.) */
    s16         shieldHealth;   /* Screen Door / Ladder shield HP */
} Zombie;

/** Look up static stats/art for a zombie type. Never NULL. */
const ZombieDef* Zombie_GetDef(ZombieType type);

/** Reset all zombie slots to inactive. */
void Zombie_ResetAll(Zombie zombies[MAX_ZOMBIES]);

/** Find a free slot and spawn a zombie of the given type into the given row.
 *  `game` may be NULL if no minigame context is needed. */
bool Zombie_Spawn(struct GameContext* game, ZombieType type, u8 row);

/**
 * Advance all zombies one frame: walking, eating whatever plant occupies
 * their current tile (dealing damage via the grid's GridCell.health), and
 * dying once health <= 0. Returns true if any zombie reached the house
 * column this frame (column < 0), signalling a lost level.
 */
bool Zombie_UpdateAll(struct GameContext* game);

/** Apply damage to the nearest active zombie in `row` at or ahead of `minX` (tile space). */
bool Zombie_DamageNearestInRow(Zombie zombies[MAX_ZOMBIES], u8 row, f32 minX, s16 damage, bool slows);

/** Read-only check: is there any living zombie in `row` at or ahead of `minX`? */
bool Zombie_AnyActiveAheadInRow(const Zombie zombies[MAX_ZOMBIES], u8 row, f32 minX);

/** Apply `damage` to every active zombie within `radiusTiles` of (col,row) -- Cherry Bomb / Doom-shroom. */
void Zombie_DamageInBlastRadius(Zombie zombies[MAX_ZOMBIES], s8 col, s8 row, s16 radiusTiles, s16 damage);

/** Damage all zombies in a given row (Jalapeno). */
void Zombie_DamageInRow(Zombie zombies[MAX_ZOMBIES], u8 row, s16 damage);

/** Freeze every active zombie on screen (Ice-shroom). */
void Zombie_FreezeAll(Zombie zombies[MAX_ZOMBIES], u32 frames);

/** Find the closest zombie to (col,row) within `rangeTiles`. Returns index or -1. */
int Zombie_ClosestToCell(const Zombie zombies[MAX_ZOMBIES], s8 col, s8 row, f32 rangeTiles);

/** Hypnotize a zombie: it becomes friendly and walks left to right. */
void Zombie_Hypnotize(Zombie* z);

/** Remove metal from a zombie (Magnet-shroom). Returns true if any was removed. */
bool Zombie_RemoveMetal(Zombie* z);

/** True if there are no active zombies left (used for level-complete checks). */
bool Zombie_NoneActive(const Zombie zombies[MAX_ZOMBIES]);

#ifdef __cplusplus
}
#endif

#endif /* PVZ_ZOMBIE_H */
