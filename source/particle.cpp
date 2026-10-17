#include <string.h>
#include <math.h>
#include <stdlib.h>
#include "particle.h"

static int FindSlot(Particle pool[MAX_PARTICLES])
{
    for (int i = 0; i < MAX_PARTICLES; i++)
        if (!pool[i].active) return i;
    return -1;
}

static void Emit(Particle pool[MAX_PARTICLES], ParticleType kind, f32 x, f32 y,
                 f32 vx, f32 vy, f32 gravity, u32 color, f32 size, f32 life)
{
    int idx = FindSlot(pool);
    if (idx < 0) return;
    Particle* p = &pool[idx];
    p->active = true;
    p->kind = kind;
    p->x = x;
    p->y = y;
    p->vx = vx;
    p->vy = vy;
    p->gravity = gravity;
    p->color = color;
    p->size = size;
    p->life = life;
    p->maxLife = life;
}

void Particle_Spawn(Particle pool[MAX_PARTICLES], ParticleType kind, f32 x, f32 y, int count)
{
    for (int i = 0; i < count; i++)
    {
        f32 angle = (f32)(rand() % 628) / 100.0f;
        f32 speed = (f32)(rand() % 100 + 30) / 100.0f;
        u32 color = 0xFFFFFFFF;
        f32 size = 4.0f;
        f32 life = (f32)(rand() % 30 + 20) / 10.0f;
        f32 gravity = 0.0f;

        switch (kind)
        {
            case PARTICLE_POOF:
                color = 0xCCCCCCFF;
                size = (f32)(rand() % 8 + 6);
                life = (f32)(rand() % 15 + 15) / 10.0f;
                speed *= 0.5f;
                break;
            case PARTICLE_SPARK:
                color = 0xFFFF44FF;
                size = (f32)(rand() % 3 + 2);
                life = (f32)(rand() % 10 + 10) / 10.0f;
                gravity = -0.5f;
                break;
            case PARTICLE_ICE:
                color = 0xAADDFFFF;
                size = (f32)(rand() % 5 + 3);
                life = (f32)(rand() % 20 + 15) / 10.0f;
                gravity = 0.3f;
                break;
            case PARTICLE_BUTTER:
                color = 0xFFDD66FF;
                size = (f32)(rand() % 4 + 3);
                life = (f32)(rand() % 15 + 10) / 10.0f;
                gravity = 1.0f;
                break;
            case PARTICLE_BLOOD:
                color = 0x33AA22FF;
                size = (f32)(rand() % 4 + 2);
                life = (f32)(rand() % 15 + 10) / 10.0f;
                gravity = 1.5f;
                break;
            case PARTICLE_SUN_SPARKLE:
                color = 0xFFDD44FF;
                size = (f32)(rand() % 3 + 1);
                life = (f32)(rand() % 10 + 5) / 10.0f;
                gravity = -0.3f;
                break;
            case PARTICLE_EXPLOSION:
                color = 0xFF6600FF;
                size = (f32)(rand() % 8 + 5);
                life = (f32)(rand() % 15 + 10) / 10.0f;
                speed *= 1.5f;
                break;
            case PARTICLE_LEAF:
                color = 0x44CC44FF;
                size = (f32)(rand() % 4 + 3);
                life = (f32)(rand() % 25 + 20) / 10.0f;
                gravity = 0.5f;
                break;
        }
        Emit(pool, kind, x, y, cosf(angle) * speed, sinf(angle) * speed, gravity, color, size, life);
    }
}

void Particle_SpawnExplosion(Particle pool[MAX_PARTICLES], f32 x, f32 y)
{
    Particle_Spawn(pool, PARTICLE_EXPLOSION, x, y, 12);
    Particle_Spawn(pool, PARTICLE_SPARK, x, y, 8);
    Particle_Spawn(pool, PARTICLE_POOF, x, y, 6);
}

void Particle_SpawnPoof(Particle pool[MAX_PARTICLES], f32 x, f32 y)
{
    Particle_Spawn(pool, PARTICLE_POOF, x, y, 6);
}

void Particle_SpawnIce(Particle pool[MAX_PARTICLES], f32 x, f32 y)
{
    Particle_Spawn(pool, PARTICLE_ICE, x, y, 8);
}

void Particle_SpawnButter(Particle pool[MAX_PARTICLES], f32 x, f32 y)
{
    Particle_Spawn(pool, PARTICLE_BUTTER, x, y, 5);
}

void Particle_Update(Particle pool[MAX_PARTICLES])
{
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        Particle* p = &pool[i];
        if (!p->active) continue;

        p->x += p->vx;
        p->vy += p->gravity;
        p->y += p->vy;
        p->life -= 1.0f / 60.0f;

        if (p->life <= 0.0f)
        {
            p->active = false;
            continue;
        }

        float fade = p->life / p->maxLife;
        u8 alpha = (u8)(fade * 255.0f);
        p->color = (p->color & 0xFFFFFF00) | alpha;
    }
}
