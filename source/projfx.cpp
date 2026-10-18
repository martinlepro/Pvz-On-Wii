/**
 * projfx.cpp - Real projectile sprites + impact effects (splats, drops,
 * melon chunks, fire trails, popcorn / cob explosion).
 *
 * SELF-CONTAINED: no struct or header changes. It only READS the existing
 * Projectile pool (types.h / game.h). Hits are detected render-side: a pool
 * slot that was active last frame and is inactive now, while still on the
 * lawn, is treated as an impact and spawns an effect. Lobbed arcs are also
 * render-only (the sim keeps its straight-line motion).
 *
 * render.cpp calls (re-declared there, C linkage):
 *     ProjFx_Load();                    in Render_Init  (one loading tick)
 *     ProjFx_Free();                    in Render_Shutdown
 *     ProjFx_BeginFrame(game);          start of DrawProjectiles
 *     ProjFx_DrawProjectile(game, i, sx, sy)  per active projectile;
 *                                       returns false -> keep the old circle
 *     ProjFx_DrawEffects(game);         after the projectile loop
 * Extra hooks for features the sim does not have yet (nothing calls them):
 *     ProjFx_CobExplosion(tileX, tileY), ProjFx_PopcornBurst(tileX, tileY)
 *     ProjFx_DrawBasketball(sx, sy, angleDeg)
 */
#include "game.h"
#include "constants.h"
#include <grrlib.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>

extern "C" GRRLIB_texImg* Render_LoadTexture(const char* relPath);

#ifndef MAX_PROJECTILES
#define MAX_PROJECTILES 48
#endif

/* ------------------------------------------------------------------ */
/* Textures                                                            */
/* ------------------------------------------------------------------ */
enum Tx {
    TX_PEA, TX_SNOW, TX_FIRE, TX_SPORE, TX_SPORE_SMALL, TX_SPIKE, TX_STAR,
    TX_CABBAGE, TX_KERNEL, TX_BUTTER, TX_BUTTER_SPLAT, TX_MELON, TX_WINTER,
    TX_FIRE_SPARK,
    TX_FIRE_TRAIL, /* 3 */
    TX_PEA_SPLAT = TX_FIRE_TRAIL + 3,     /* 4 */
    TX_PEA_DROP  = TX_PEA_SPLAT + 4,      /* 3 */
    TX_SNOW_SPLAT = TX_PEA_DROP + 3,      /* 4 */
    TX_STAR_SPLAT = TX_SNOW_SPLAT + 4,    /* 4 */
    TX_STAR_DROP  = TX_STAR_SPLAT + 4,    /* 3 */
    TX_MELON_CHUNK = TX_STAR_DROP + 3,    /* 9 */
    TX_WINTER_CHUNK = TX_MELON_CHUNK + 9, /* 9 */
    /* lazily loaded (Cob Cannon / catapult: no sim hooks yet) */
    TX_COB = TX_WINTER_CHUNK + 9,
    TX_COB_EXPLOSION,
    TX_POPCORN,                           /* 5 */
    TX_BASKETBALL = TX_POPCORN + 5,
    TX_COUNT
};
#define TX_EAGER_END TX_COB

static GRRLIB_texImg* s_tx[TX_COUNT];
static bool s_cobLoaded = false;

static GRRLIB_texImg* LoadP(const char* name)
{
    char path[96];
    snprintf(path, sizeof(path), "assets/projectiles/%s.png", name);
    return Render_LoadTexture(path);
}

static void LoadSeq(int base, const char* fmt, int n)
{
    for (int i = 0; i < n; ++i)
    {
        char nm[48];
        snprintf(nm, sizeof(nm), fmt, i + 1);
        s_tx[base + i] = LoadP(nm);
    }
}

extern "C" void ProjFx_Load(void)
{
    memset(s_tx, 0, sizeof(s_tx));
    s_tx[TX_PEA]          = LoadP("pea");
    s_tx[TX_SNOW]         = LoadP("snow_pea");
    s_tx[TX_FIRE]         = LoadP("fire_pea");
    s_tx[TX_SPORE]        = LoadP("spore");
    s_tx[TX_SPORE_SMALL]  = LoadP("spore_small");
    s_tx[TX_SPIKE]        = LoadP("spike");
    s_tx[TX_STAR]         = LoadP("star");
    s_tx[TX_CABBAGE]      = LoadP("cabbage");
    s_tx[TX_KERNEL]       = LoadP("kernel");
    s_tx[TX_BUTTER]       = LoadP("butter");
    s_tx[TX_BUTTER_SPLAT] = LoadP("butter_splat");
    s_tx[TX_MELON]        = LoadP("melon");
    s_tx[TX_WINTER]       = LoadP("winter_melon");
    s_tx[TX_FIRE_SPARK]   = LoadP("fire_spark");
    LoadSeq(TX_FIRE_TRAIL,   "fire_trail_%d", 3);
    LoadSeq(TX_PEA_SPLAT,    "pea_splat_%d", 4);
    LoadSeq(TX_PEA_DROP,     "pea_drop_%d", 3);
    LoadSeq(TX_SNOW_SPLAT,   "snow_splat_%d", 4);
    LoadSeq(TX_STAR_SPLAT,   "star_splat_%d", 4);
    LoadSeq(TX_STAR_DROP,    "star_drop_%d", 3);
    LoadSeq(TX_MELON_CHUNK,  "melon_chunk_%d", 9);
    LoadSeq(TX_WINTER_CHUNK, "winter_melon_chunk_%d", 9);
    s_cobLoaded = false;
}

static void EnsureCob(void)
{
    if (s_cobLoaded) return;
    s_cobLoaded = true;
    s_tx[TX_COB]           = LoadP("cob");
    s_tx[TX_COB_EXPLOSION] = LoadP("cob_explosion");
    LoadSeq(TX_POPCORN, "popcorn_%d", 5);
    s_tx[TX_BASKETBALL]    = LoadP("basketball");
}

extern "C" void ProjFx_Free(void)
{
    for (int i = 0; i < TX_COUNT; ++i)
        if (s_tx[i]) { GRRLIB_FreeTexture(s_tx[i]); s_tx[i] = NULL; }
    s_cobLoaded = false;
}

/* ------------------------------------------------------------------ */
/* Drawing helper (explicit rotated quad; no GRRLIB_SetHandle)         */
/* ------------------------------------------------------------------ */
static void DrawQuad(GRRLIB_texImg* tex, f32 cx, f32 cy, f32 sx, f32 sy,
                     f32 angleDeg, u32 color)
{
    if (!tex || !tex->data) return;
    const f32 hw = tex->w * sx * 0.5f, hh = tex->h * sy * 0.5f;
    const f32 rad = angleDeg * 3.14159265f / 180.0f;
    const f32 c = cosf(rad), s = sinf(rad);
    const f32 lx[4] = { -hw,  hw, hw, -hw };
    const f32 ly[4] = { -hh, -hh, hh,  hh };
    guVector q[4];
    for (int i = 0; i < 4; ++i)
    {
        q[i].x = cx + lx[i] * c - ly[i] * s;
        q[i].y = cy + lx[i] * s + ly[i] * c;
        q[i].z = 0.0f;
    }
    GRRLIB_DrawImgQuad(q, tex, color);
}

static u32 WithAlpha(u32 rgba, f32 a01)
{
    if (a01 < 0.0f) a01 = 0.0f;
    if (a01 > 1.0f) a01 = 1.0f;
    return (rgba & 0xFFFFFF00u) | (u32)((rgba & 0xFF) * a01);
}

/* Sprites were drawn for an ~80px-wide cell; scale to the real cell. */
static f32 PxScale(const GameContext* g)
{
    f32 s = (f32)g->cellW / 80.0f;
    return s < 0.5f ? 0.5f : s;
}

/* ------------------------------------------------------------------ */
/* Per-slot tracking (hit detection, arcs)                             */
/* ------------------------------------------------------------------ */
struct Track {
    bool active;
    ProjectileKind kind;
    f32 x, y, dx, dy, spawnX;
    u32 age;
};
static Track s_trk[MAX_PROJECTILES];

static bool IsLobbed(ProjectileKind k)
{
    return k == PROJ_CABBAGE || k == PROJ_KERNEL || k == PROJ_BUTTER ||
           k == PROJ_MELON || k == PROJ_WINTER;
}

/* Arc: hump over ARC_TILES of travel, ARC_PX high (scaled). */
#define ARC_TILES 4.0f
#define ARC_PX    46.0f
static f32 ArcLift(const GameContext* g, const Track* t, f32 x)
{
    f32 prog = (x - t->spawnX) / ARC_TILES;
    if (prog < 0.0f) prog = 0.0f;
    if (prog > 1.0f) prog = 1.0f;
    return sinf(prog * 3.14159265f) * ARC_PX * PxScale(g);
}

/* ------------------------------------------------------------------ */
/* Effects pool (screen-space pixels)                                  */
/* ------------------------------------------------------------------ */
enum FxKind { FX_ANIM, FX_FLY, FX_FADE };
struct Fx {
    bool active;
    FxKind kind;
    int  tex;        /* base texture index */
    int  frames;     /* FX_ANIM: number of frames in sequence (textures contiguous) */
    f32  x, y, vx, vy, grav;
    f32  rot, vrot, scale;
    u32  age, life;
    u32  color;
};
#define MAX_FX 80
static Fx s_fx[MAX_FX];

static Fx* NewFx(void)
{
    for (int i = 0; i < MAX_FX; ++i)
        if (!s_fx[i].active) { memset(&s_fx[i], 0, sizeof(Fx)); s_fx[i].active = true; return &s_fx[i]; }
    return NULL;
}

static f32 Rnd(f32 lo, f32 hi) { return lo + (hi - lo) * ((f32)(rand() % 1000) / 999.0f); }

static void SpawnAnim(int texBase, int frames, f32 x, f32 y, f32 scale, u32 life, u32 color)
{
    Fx* f = NewFx(); if (!f) return;
    f->kind = FX_ANIM; f->tex = texBase; f->frames = frames;
    f->x = x; f->y = y; f->scale = scale; f->life = life; f->color = color;
}

static void SpawnFlyer(int tex, f32 x, f32 y, f32 vx, f32 vy, f32 grav,
                       f32 scale, u32 life, u32 color)
{
    Fx* f = NewFx(); if (!f) return;
    f->kind = FX_FLY; f->tex = tex; f->x = x; f->y = y; f->vx = vx; f->vy = vy;
    f->grav = grav; f->scale = scale; f->life = life; f->color = color;
    f->rot = Rnd(0, 360); f->vrot = Rnd(-14, 14);
}

static void SpawnFade(int tex, f32 x, f32 y, f32 scale, u32 life, u32 color)
{
    Fx* f = NewFx(); if (!f) return;
    f->kind = FX_FADE; f->tex = tex; f->x = x; f->y = y; f->scale = scale;
    f->life = life; f->color = color;
}

static void Drops(int base, int variants, f32 x, f32 y, f32 sc, int n, u32 color)
{
    for (int i = 0; i < n; ++i)
        SpawnFlyer(base + rand() % variants, x, y, Rnd(-2.2f, 2.2f), Rnd(-3.4f, -1.2f),
                   0.22f, sc, 22 + rand() % 10, color);
}

static void Chunks(int base, f32 x, f32 y, f32 sc)
{
    for (int i = 0; i < 9; ++i)
        SpawnFlyer(base + i, x, y, Rnd(-3.0f, 3.0f), Rnd(-4.5f, -1.5f),
                   0.28f, sc, 34 + rand() % 14, 0xFFFFFFFF);
}

/* An impact of `kind` at screen (x,y). */
static void SpawnImpact(const GameContext* g, ProjectileKind kind, f32 x, f32 y)
{
    const f32 sc = PxScale(g);
    switch (kind)
    {
        case PROJ_PEA:
        case PROJ_HOMING:
            SpawnAnim(TX_PEA_SPLAT, 4, x, y, sc, 14, 0xFFFFFFFF);
            Drops(TX_PEA_DROP, 3, x, y, sc, 3, 0xFFFFFFFF);
            break;
        case PROJ_SNOW:
            SpawnAnim(TX_SNOW_SPLAT, 4, x, y, sc, 14, 0xFFFFFFFF);
            Drops(TX_PEA_DROP, 3, x, y, sc, 3, 0xBFEFFFFF);
            break;
        case PROJ_FIRE:
            SpawnAnim(TX_PEA_SPLAT, 4, x, y, sc, 14, 0xFF9A3AFF);
            for (int i = 0; i < 5; ++i)
                SpawnFlyer(TX_FIRE_SPARK, x, y, Rnd(-2.8f, 2.8f), Rnd(-3.2f, -0.6f),
                           0.12f, sc * 1.4f, 18 + rand() % 8, 0xFFFFFFFF);
            break;
        case PROJ_STAR:
            SpawnAnim(TX_STAR_SPLAT, 4, x, y, sc, 14, 0xFFFFFFFF);
            Drops(TX_STAR_DROP, 3, x, y, sc, 3, 0xFFFFFFFF);
            break;
        case PROJ_SPIKE:
            Drops(TX_PEA_DROP, 3, x, y, sc, 2, 0xB08A5AFF);
            break;
        case PROJ_SPORE:
        case PROJ_FUME:
            SpawnFade(TX_SPORE_SMALL, x, y, sc * 1.2f, 16, 0xFFFFFFFF);
            break;
        case PROJ_CABBAGE:
            SpawnAnim(TX_PEA_SPLAT, 4, x, y, sc * 1.3f, 14, 0x7CCB3AFF);
            Drops(TX_PEA_DROP, 3, x, y, sc, 4, 0x7CCB3AFF);
            break;
        case PROJ_KERNEL:
            Drops(TX_PEA_DROP, 3, x, y, sc, 4, 0xFFD34DFF);
            break;
        case PROJ_BUTTER:
            SpawnFade(TX_BUTTER_SPLAT, x, y, sc, 36, 0xFFFFFFFF);
            break;
        case PROJ_MELON:
            Chunks(TX_MELON_CHUNK, x, y, sc);
            break;
        case PROJ_WINTER:
            Chunks(TX_WINTER_CHUNK, x, y, sc);
            break;
        default:
            break;
    }
}

extern "C" void ProjFx_BeginFrame(const GameContext* game)
{
    if (!game) return;
    if (game->state != GAME_STATE_PLAYING)
    {
        /* Level start/end resets the pool: never turn that into impacts. */
        for (int i = 0; i < MAX_PROJECTILES; ++i) s_trk[i].active = false;
        return;
    }
    for (int i = 0; i < MAX_PROJECTILES; ++i)
    {
        const Projectile* p = &game->projectiles[i];
        Track* t = &s_trk[i];
        if (p->active)
        {
            if (!t->active || fabsf(p->x - t->x) > 1.0f)
            { t->spawnX = p->x; t->age = 0; }   /* new shot (or slot reused) */
            t->active = true; t->kind = p->kind;
            t->x = p->x; t->y = p->y; t->dx = p->dx; t->dy = p->dy;
            t->age++;
        }
        else if (t->active)
        {
            t->active = false;
            /* Vanished while on the lawn = impact (off-screen = just left). */
            if (t->x > -0.3f && t->x < (f32)GRID_COLS + 0.6f)
            {
                f32 hx = t->x + t->dx * 0.1f, hy = t->y;
                f32 sx = game->lawnX + hx * game->cellW;
                f32 sy = game->lawnY + hy * game->cellH;
                if (IsLobbed(t->kind)) sy -= game->cellH * 0.2f;
                SpawnImpact(game, t->kind, sx, sy);
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* Projectile sprites                                                  */
/* ------------------------------------------------------------------ */
extern "C" bool ProjFx_DrawProjectile(const GameContext* game, int i, f32 sx, f32 sy)
{
    const Projectile* p = &game->projectiles[i];
    const Track* t = &s_trk[i];
    const f32 sc = PxScale(game);
    const f32 ang = atan2f(p->dy, p->dx) * 180.0f / 3.14159265f;
    const f32 spin = (f32)game->frameCount * 9.0f;

    GRRLIB_texImg* tex = NULL;
    f32 scale = sc, angle = 0.0f, lift = 0.0f;
    u32 color = 0xFFFFFFFF;

    switch (p->kind)
    {
        case PROJ_PEA:     tex = s_tx[TX_PEA];   break;
        case PROJ_SNOW:    tex = s_tx[TX_SNOW];  break;
        case PROJ_FIRE:
        {
            tex = s_tx[TX_FIRE];
            /* flickering trail behind the pea */
            GRRLIB_texImg* tr = s_tx[TX_FIRE_TRAIL + (int)((game->frameCount / 3) % 3)];
            if (tr && tex)
                DrawQuad(tr, sx - tr->w * sc * 0.55f, sy, sc, sc, 0.0f, 0xFFFFFFFF);
            break;
        }
        case PROJ_FUME:
            tex = s_tx[TX_SPORE]; scale = sc * (1.2f + 0.25f * sinf(spin * 0.05f));
            color = 0xB6E86AD0; break;
        case PROJ_SPORE:
            tex = s_tx[TX_SPORE_SMALL]; scale = sc; break;
        case PROJ_STAR:    tex = s_tx[TX_STAR];  angle = spin; break;
        case PROJ_SPIKE:
        case PROJ_HOMING:  tex = s_tx[TX_SPIKE]; angle = ang;  break;
        case PROJ_CABBAGE: tex = s_tx[TX_CABBAGE]; angle = spin * 0.6f; lift = ArcLift(game, t, p->x); break;
        case PROJ_KERNEL:  tex = s_tx[TX_KERNEL];  angle = spin * 0.8f; lift = ArcLift(game, t, p->x); break;
        case PROJ_BUTTER:  tex = s_tx[TX_BUTTER];  angle = spin * 0.5f; lift = ArcLift(game, t, p->x); break;
        case PROJ_MELON:   tex = s_tx[TX_MELON];   angle = spin * 0.4f; lift = ArcLift(game, t, p->x); break;
        case PROJ_WINTER:  tex = s_tx[TX_WINTER];  angle = spin * 0.4f; lift = ArcLift(game, t, p->x); break;
        default: return false;
    }
    if (!tex || !tex->data) return false; /* caller draws the old circle */

    if (lift > 0.0f)
    {
        /* little ground shadow so the arc reads */
        GRRLIB_Circle(sx, sy + game->cellH * 0.2f, 4.0f * sc, 0x00000040, true);
    }
    DrawQuad(tex, sx, sy - lift, scale, scale, angle, color);
    return true;
}

/* ------------------------------------------------------------------ */
/* Effects update + draw                                               */
/* ------------------------------------------------------------------ */
extern "C" void ProjFx_DrawEffects(const GameContext* game)
{
    (void)game;
    for (int i = 0; i < MAX_FX; ++i)
    {
        Fx* f = &s_fx[i];
        if (!f->active) continue;
        if (f->age >= f->life) { f->active = false; continue; }
        const f32 u = (f32)f->age / (f32)f->life;      /* 0..1 */

        switch (f->kind)
        {
            case FX_ANIM:
            {
                int fr = (int)(u * f->frames);
                if (fr >= f->frames) fr = f->frames - 1;
                DrawQuad(s_tx[f->tex + fr], f->x, f->y, f->scale, f->scale, 0.0f,
                         WithAlpha(f->color, 1.0f - u * u));
                break;
            }
            case FX_FADE:
                DrawQuad(s_tx[f->tex], f->x, f->y, f->scale * (1.0f + 0.4f * u),
                         f->scale * (1.0f + 0.4f * u), 0.0f, WithAlpha(f->color, 1.0f - u));
                break;
            case FX_FLY:
                f->x += f->vx; f->y += f->vy; f->vy += f->grav; f->rot += f->vrot;
                DrawQuad(s_tx[f->tex], f->x, f->y, f->scale, f->scale, f->rot,
                         WithAlpha(f->color, u < 0.6f ? 1.0f : (1.0f - u) / 0.4f));
                break;
        }
        f->age++;
    }
}

/* ------------------------------------------------------------------ */
/* Hooks for features the simulation does not have yet                 */
/* ------------------------------------------------------------------ */
extern "C" void ProjFx_PopcornBurst(f32 sx, f32 sy)
{
    EnsureCob();
    for (int i = 0; i < 14; ++i)
        SpawnFlyer(TX_POPCORN + rand() % 5, sx, sy, Rnd(-4.0f, 4.0f), Rnd(-6.0f, -2.0f),
                   0.3f, 0.9f, 36 + rand() % 14, 0xFFFFFFFF);
}

extern "C" void ProjFx_CobExplosion(f32 sx, f32 sy)
{
    EnsureCob();
    SpawnAnim(TX_COB_EXPLOSION, 1, sx, sy, 1.0f, 26, 0xFFFFFFFF);
    ProjFx_PopcornBurst(sx, sy);
}

extern "C" void ProjFx_DrawBasketball(f32 sx, f32 sy, f32 angleDeg)
{
    EnsureCob();
    if (s_tx[TX_BASKETBALL]) DrawQuad(s_tx[TX_BASKETBALL], sx, sy, 0.8f, 0.8f, angleDeg, 0xFFFFFFFF);
    else GRRLIB_Circle(sx, sy, 8, 0xE07A20FF, true);
}
