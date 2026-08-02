#ifndef PVZ_PARTICLE_H
#define PVZ_PARTICLE_H

#ifdef __cplusplus
extern "C" {
#endif

#include <gctypes.h>

#define MAX_PARTICLES 64

typedef enum {
    PARTICLE_POOF,
    PARTICLE_SPARK,
    PARTICLE_ICE,
    PARTICLE_BUTTER,
    PARTICLE_BLOOD,
    PARTICLE_SUN_SPARKLE,
    PARTICLE_EXPLOSION,
    PARTICLE_LEAF,
    PARTICLE_TYPE_COUNT
} ParticleType;

typedef struct {
    bool  active;
    f32   x, y;
    f32   vx, vy;
    f32   gravity;
    u32   color;
    f32   size;
    f32   life;
    f32   maxLife;
    ParticleType kind;
} Particle;

void Particle_Spawn(Particle pool[MAX_PARTICLES], ParticleType kind, f32 x, f32 y, int count);
void Particle_SpawnExplosion(Particle pool[MAX_PARTICLES], f32 x, f32 y);
void Particle_SpawnPoof(Particle pool[MAX_PARTICLES], f32 x, f32 y);
void Particle_SpawnIce(Particle pool[MAX_PARTICLES], f32 x, f32 y);
void Particle_SpawnButter(Particle pool[MAX_PARTICLES], f32 x, f32 y);
void Particle_Update(Particle pool[MAX_PARTICLES]);

#ifdef __cplusplus
}
#endif

#endif
