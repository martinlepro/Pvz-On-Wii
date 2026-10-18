/**
 * types.h - Shared enums and lightweight structs.
 */
#ifndef PVZ_TYPES_H
#define PVZ_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <gctypes.h>

/** Plant types (matches the original PvZ Adventure Mode unlock order). */
typedef enum {
    PLANT_NONE = -1,
    PLANT_PEASHOOTER = 0,
    PLANT_SUNFLOWER,
    PLANT_CHERRY_BOMB,
    PLANT_WALLNUT,
    PLANT_POTATO_MINE,
    PLANT_SNOW_PEA,
    PLANT_CHOMPER,
    PLANT_REPEATER,
    PLANT_PUFF_SHROOM,
    PLANT_SUN_SHROOM,
    PLANT_FUME_SHROOM,
    PLANT_GRAVE_BUSTER,
    PLANT_HYPNO_SHROOM,
    PLANT_SCAREDY_SHROOM,
    PLANT_ICE_SHROOM,
    PLANT_DOOM_SHROOM,
    PLANT_LILY_PAD,
    PLANT_SQUASH,
    PLANT_THREEPEATER,
    PLANT_TANGLE_KELP,
    PLANT_JALAPENO,
    PLANT_SPIKEWEED,
    PLANT_TORCHWOOD,
    PLANT_TALLNUT,
    PLANT_SEA_SHROOM,
    PLANT_PLANTERN,
    PLANT_CACTUS,
    PLANT_BLOVER,
    PLANT_SPLIT_PEA,
    PLANT_STARFRUIT,
    PLANT_PUMPKIN,
    PLANT_MAGNET_SHROOM,
    PLANT_CABBAGE_PULT,
    PLANT_FLOWER_POT,
    PLANT_KERNEL_PULT,
    PLANT_COFFEE_BEAN,
    PLANT_GARLIC,
    PLANT_UMBRELLA_LEAF,
    PLANT_MARIGOLD,
    PLANT_MELON_PULT,
    PLANT_GATLING_PEA,
    PLANT_TWIN_SUNFLOWER,
    PLANT_GLOOM_SHROOM,
    PLANT_CATTAIL,
    PLANT_WINTER_MELON,
    PLANT_GOLD_MAGNET,
    PLANT_SPIKEROCK,
    PLANT_COB_CANNON,
    PLANT_IMITATER,
    PLANT_TYPE_COUNT
} PlantType;

/** Recharge speed categories for the seed bar cooldown. */
typedef enum {
    RECHARGE_FAST = 0,
    RECHARGE_MEDIUM,
    RECHARGE_SLOW,
    RECHARGE_VERY_SLOW,
} RechargeSpeed;

/** High-level game states. */
typedef enum {
    GAME_STATE_SELECTING = 0, /* choosing plants before a level */
    GAME_STATE_PLAYING,
    GAME_STATE_PAUSED,
    GAME_STATE_LEVEL_WON,
    GAME_STATE_GAME_OVER
} GameState;

/** Single grid cell on the lawn. */
typedef struct {
    PlantType plant;
    s16       health;
    u16       shootTimer;
    bool      hasLilyPad;
    bool      hasFlowerPot;
    bool      hasGrave;
    bool      hasPumpkin;
    s16       pumpkinHealth;
    bool      hasLadder;
} GridCell;

/** World-specific modifiers. */
typedef enum {
    WORLD_FLAG_SUN_FALLS   = 1 << 0, /* sun drops from the sky         */
    WORLD_FLAG_HAS_WATER   = 1 << 1, /* middle lanes are water         */
    WORLD_FLAG_HAS_FOG     = 1 << 2, /* fog covers parts of the lawn  */
    WORLD_FLAG_NEEDS_POTS  = 1 << 3, /* roof: must plant in pots      */
    WORLD_FLAG_HAS_GRAVES  = 1 << 4, /* night: pre-placed graves      */
} WorldFlags;

/** 2D floating-point position for smooth cursor movement. */
typedef struct {
    f32 x;
    f32 y;
} Vec2;

/** RGBA color for GRRLIB drawing helpers. */
typedef struct {
    u8 r, g, b, a;
} Color4;

typedef enum {
    SUN_PHASE_FALLING = 0,
    SUN_PHASE_RISING,
    SUN_PHASE_DROPPING,
    SUN_PHASE_RESTING
} SunPhase;

/** A single falling sun the player can collect by pointing the Wiimote at it. */
typedef struct {
    bool active;
    bool fromSunflower;
    f32  x, y;         /* current on-screen position */
    f32  targetY;      /* resting height once it stops falling */
    f32  peakY;        /* highest point during sunflower rise animation */
    u32  restFrames;   /* how long it has been resting (for lifetime expiry) */
    SunPhase phase;
} SunDrop;

typedef enum {
    PROJ_PEA,
    PROJ_SNOW,
    PROJ_FIRE,       /* Torchwood-buffed pea (2x damage) */
    PROJ_FUME,       /* Pierces all zombies in row */
    PROJ_SPORE,      /* Puff/Sea-shroom (short range) */
    PROJ_STAR,       /* Starfruit (travels in direction) */
    PROJ_CABBAGE,    /* Lobbed, shield-ignoring */
    PROJ_KERNEL,     /* Lobbed, chance to butter (stun) */
    PROJ_BUTTER,     /* Stun projectile from kernel */
    PROJ_MELON,      /* Lobbed splash */
    PROJ_WINTER,     /* Lobbed splash + slow */
    PROJ_SPIKE,      /* Spikeweed/Spikerock / Cactus */
    PROJ_HOMING,     /* Cattail homing shot */
} ProjectileKind;

/** A projectile fired by a shooter plant. */
typedef struct {
    bool  active;
    u8    row;
    f32   x;          /* tile-space X, fractional */
    f32   y;          /* tile-space Y (for lobbed arcs) */
    f32   dx, dy;     /* direction per frame (starfruit, homing) */
    s16   damage;
    ProjectileKind kind;
    bool  slows;
    bool  pierces;    /* hits all zombies in row (fume) */
    u8    splashRadius;
    u8    life;       /* remaining range in tiles (spore=3, fume=~7) */
} Projectile;

#ifdef __cplusplus
}
#endif

#endif /* PVZ_TYPES_H */
