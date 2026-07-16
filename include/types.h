/**
 * types.h - Shared enums and lightweight structs.
 */
#ifndef PVZ_TYPES_H
#define PVZ_TYPES_H

#ifdef __cplusplus
extern "C" {
#endif

#include <ogc/types.h>

/** Plant types available in the seed bar (order matches bar slots). */
typedef enum {
    PLANT_NONE = -1,
    PLANT_PEASHOOTER = 0,
    PLANT_SUNFLOWER,
    PLANT_WALLNUT,
    PLANT_CHERRY_BOMB,
    PLANT_REPEATER,
    PLANT_SNOW_PEA,
    PLANT_TYPE_COUNT
} PlantType;

/** High-level game states for future menu/pause expansion. */
typedef enum {
    GAME_STATE_PLAYING = 0,
    GAME_STATE_PAUSED,
    GAME_STATE_GAME_OVER
} GameState;

/** Single grid cell on the lawn. */
typedef struct {
    PlantType plant;
    u8        health;       /* 0 = empty slot uses PLANT_NONE */
    u8        shootTimer;   /* frames until next pea (future use) */
} GridCell;

/** 2D floating-point position for smooth cursor movement. */
typedef struct {
    f32 x;
    f32 y;
} Vec2;

/** RGBA color for GRRLIB drawing helpers. */
typedef struct {
    u8 r, g, b, a;
} Color4;

/** A single falling sun the player can collect by pointing the Wiimote at it. */
typedef struct {
    bool active;
    f32  x, y;         /* current on-screen position */
    f32  targetY;      /* resting height once it stops falling */
    u32  restFrames;   /* how long it has been resting (for lifetime expiry) */
} SunDrop;

#ifdef __cplusplus
}
#endif

#endif /* PVZ_TYPES_H */
