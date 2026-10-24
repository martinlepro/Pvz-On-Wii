/**
 * mower.cpp - Last line of defence: one lawn mower / pool cleaner / roof
 * cleaner per row. Idle at the left edge; the first zombie to reach the
 * trigger line in a row sets it rolling across the lawn, killing everything
 * in that row, then it is gone for the rest of the level.
 *
 * SELF-CONTAINED: no struct/header changes; state is file-local.
 * Hooks (each re-declared locally in the calling file):
 *   level.cpp   Level_StartPlaying():       Mower_Reset(game);
 *   game.cpp    Game_Update(), PLAYING, just before Zombie_UpdateAll():
 *                                           Mower_Update(game);
 *   render.cpp  Render_Frame(), after DrawZombies(): Mower_Draw(game);
 *
 * Which machine goes where:
 *   roof worlds (WORLD_FLAG_NEEDS_POTS)  -> roof cleaner on every row
 *   water worlds (WORLD_FLAG_HAS_WATER)  -> pool cleaner on rows 2 and 3
 *   everything else                      -> lawn mower
 * Tricked-out skin: create an empty file  sd:/apps/pvz_wii/trickedout
 * (or call Mower_SetTrickedOut(true)); it re-skins the lawn mowers only.
 *
 * Art (relative to assets/): lawnmower/, lawnmower_trickedout/,
 * pool_cleaner/, roof_cleaner/. Parts are loaded per world at level start
 * and any missing PNG falls back to a flat coloured block.
 */
#include "game.h"
#include "constants.h"
#include "zombie.h"
#include "particle.h"
#include "survival.h"
#include "sfx.h"
#include <grrlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

extern "C" GRRLIB_texImg* Render_LoadTexture(const char* relPath);

#ifndef SETTINGS_DIR
#define SETTINGS_DIR "sd:/apps/pvz_wii"
#endif

#define MOWER_START_X     (-0.62f)   /* tile units; lawn starts at 0     */
#define MOWER_TRIGGER_X   0.45f      /* zombie this close to the house   */
#define MOWER_SPEED       0.115f     /* tiles per frame (~6.9 tiles/sec) */
#define MOWER_KILL_RANGE  0.60f

enum Kind { K_LAWN = 0, K_POOL, K_ROOF, K_COUNT };

struct Mower {
    bool present;
    bool running;
    u8   kind;
    f32  x;
};
static Mower s_m[GRID_ROWS];
static bool  s_ready = false;       /* a level is active: draw/update */
static bool  s_tricked = false;
static bool  s_trickChecked = false;
static int   s_level = -1;

/* ---- textures ---- */
enum {
    /* lawn: */ T_BODY, T_ENGINE, T_WHEEL_A, T_WHEEL_B,
    /* tricked-out extras: */ T_TBODY, T_TENGINE, T_TPIPES, T_DICE,
    /* pool: */ T_PBODY, T_PFUNNEL, T_PWHEEL,
    /* roof: */ T_RBODY, T_RCLOCK, T_BRUSH /* 5 */,
    T_COUNT = T_BRUSH + 5
};
static GRRLIB_texImg* s_t[T_COUNT];
static bool s_loaded[K_COUNT];

static GRRLIB_texImg* Ld(const char* p) { return Render_LoadTexture(p); }

static void FreeRange(int a, int b)
{
    for (int i = a; i < b; ++i)
        if (s_t[i]) { GRRLIB_FreeTexture(s_t[i]); s_t[i] = NULL; }
}

static void LoadKind(int k)
{
    if (s_loaded[k]) return;
    s_loaded[k] = true;
    char p[96];
    switch (k)
    {
        case K_LAWN:
            s_t[T_BODY]    = Ld("assets/lawnmower/body.png");
            s_t[T_ENGINE]  = Ld("assets/lawnmower/engine.png");
            s_t[T_WHEEL_A] = Ld("assets/lawnmower/wheel_1.png");
            s_t[T_WHEEL_B] = Ld("assets/lawnmower/wheel_4.png");
            if (s_tricked)
            {
                s_t[T_TBODY]   = Ld("assets/lawnmower_trickedout/body.png");
                s_t[T_TENGINE] = Ld("assets/lawnmower_trickedout/engine.png");
                s_t[T_TPIPES]  = Ld("assets/lawnmower_trickedout/engine_pipes.png");
                s_t[T_DICE]    = Ld("assets/lawnmower_trickedout/dice.png");
            }
            break;
        case K_POOL:
            s_t[T_PBODY]   = Ld("assets/pool_cleaner/body.png");
            s_t[T_PFUNNEL] = Ld("assets/pool_cleaner/funnel_1.png");
            s_t[T_PWHEEL]  = Ld("assets/pool_cleaner/wheel.png");
            break;
        case K_ROOF:
            s_t[T_RBODY]  = Ld("assets/roof_cleaner/body.png");
            s_t[T_RCLOCK] = Ld("assets/roof_cleaner/clock.png");
            for (int i = 0; i < 5; ++i)
            {
                snprintf(p, sizeof(p), "assets/roof_cleaner/brush_%d.png", i + 1);
                s_t[T_BRUSH + i] = Ld(p);
            }
            break;
    }
}

static void FreeKind(int k)
{
    if (!s_loaded[k]) return;
    s_loaded[k] = false;
    if (k == K_LAWN) FreeRange(T_BODY, T_DICE + 1);
    if (k == K_POOL) FreeRange(T_PBODY, T_PWHEEL + 1);
    if (k == K_ROOF) FreeRange(T_RBODY, T_COUNT);
}

extern "C" void Mower_SetTrickedOut(bool on)
{
    s_tricked = on;
    s_trickChecked = true;
    FreeKind(K_LAWN); /* reload with the right skin at next level start */
}

static void CheckTrickFlag(void)
{
    if (s_trickChecked) return;
    s_trickChecked = true;
    FILE* f = fopen(SETTINGS_DIR "/trickedout", "r");
    if (f) { s_tricked = true; fclose(f); }
}

/* ------------------------------------------------------------------ */
extern "C" void Mower_Reset(const GameContext* game)
{
    if (!game) return;
    /* Survival re-selection between flags must not hand out fresh mowers. */
    if (s_ready && s_level == game->currentLevelIndex &&
        Survival_IsSurvival((u8)game->currentLevelIndex) && game->survivalFlags > 0)
        return;

    CheckTrickFlag();
    memset(s_m, 0, sizeof(s_m));
    bool need[K_COUNT] = { false, false, false };
    const bool roof  = (game->worldFlags & WORLD_FLAG_NEEDS_POTS) != 0;
    const bool water = (game->worldFlags & WORLD_FLAG_HAS_WATER) != 0;
    for (int r = 0; r < game->rowCount && r < GRID_ROWS; ++r)
    {
        u8 k = roof ? K_ROOF : ((water && (r == 2 || r == 3)) ? K_POOL : K_LAWN);
        s_m[r].present = true;
        s_m[r].kind = k;
        s_m[r].x = MOWER_START_X;
        need[k] = true;
    }
    for (int k = 0; k < K_COUNT; ++k)
    {
        if (need[k]) LoadKind(k); else FreeKind(k);
    }
    s_level = game->currentLevelIndex;
    s_ready = true;
}

extern "C" void Mower_Update(GameContext* game)
{
    if (!game || !s_ready) return;
    for (int r = 0; r < game->rowCount && r < GRID_ROWS; ++r)
    {
        Mower* m = &s_m[r];
        if (!m->present) continue;

        if (!m->running)
        {
            for (int zi = 0; zi < MAX_ZOMBIES; ++zi)
            {
                const Zombie* z = &game->zombies[zi];
                if (!z->active || z->row != r) continue;
                if (z->state == ZSTATE_DYING || z->state == ZSTATE_FLYING ||
                    z->state == ZSTATE_DIGGING) continue;
                if (z->x < MOWER_TRIGGER_X)
                {
                    m->running = true;
                    Sfx_Play(m->kind == K_POOL ? SFX_POOL_CLEANER : SFX_LAWNMOWER);
                    break;
                }
            }
            continue;
        }

        m->x += MOWER_SPEED;
        for (int zi = 0; zi < MAX_ZOMBIES; ++zi)
        {
            Zombie* z = &game->zombies[zi];
            if (!z->active || z->row != r) continue;
            if (z->state == ZSTATE_DYING || z->state == ZSTATE_FLYING ||
                z->state == ZSTATE_DIGGING) continue;
            if (z->x > m->x + MOWER_KILL_RANGE) continue;   /* not reached yet */
            z->health = 0;
            z->state = ZSTATE_DYING;
            z->deathTimer = ZOMBIE_DEATH_FADE_FRAMES;
            Particle_SpawnPoof(game->particles, z->x, (f32)r + 0.5f);
        }
        if (m->x > (f32)GRID_COLS + 1.2f)
        {
            m->present = false;
            m->running = false;
        }
    }
}

/* ------------------------------------------------------------------ */
static void Quad(GRRLIB_texImg* t, f32 cx, f32 cy, f32 s, f32 angleDeg, u32 color)
{
    if (!t || !t->data) return;
    const f32 hw = t->w * s * 0.5f, hh = t->h * s * 0.5f;
    const f32 rad = angleDeg * 3.14159265f / 180.0f;
    const f32 c = cosf(rad), sn = sinf(rad);
    const f32 lx[4] = { -hw, hw, hw, -hw }, ly[4] = { -hh, -hh, hh, hh };
    guVector q[4];
    for (int i = 0; i < 4; ++i)
    {
        q[i].x = cx + lx[i] * c - ly[i] * sn;
        q[i].y = cy + lx[i] * sn + ly[i] * c;
        q[i].z = 0.0f;
    }
    GRRLIB_DrawImgQuad(q, t, color);
}

/* Part whose top-left sits at (ox,oy) inside a box whose top-left is (bx,by). */
static void Part(GRRLIB_texImg* t, f32 bx, f32 by, f32 ox, f32 oy, f32 s, f32 ang)
{
    if (!t || !t->data) return;
    Quad(t, bx + (ox + t->w * 0.5f) * s, by + (oy + t->h * 0.5f) * s, s, ang, 0xFFFFFFFF);
}

static void DrawOne(const GameContext* g, int row, const Mower* m)
{
    const f32 sx = g->lawnX + m->x * g->cellW;
    const f32 base = g->lawnY + row * g->cellH + g->cellH * 0.5f + g->cellH * 0.30f;
    const f32 spin = m->running ? m->x * 500.0f : 0.0f;
    const f32 jig  = m->running ? ((g->frameCount & 2) ? 1.0f : 0.0f) : 0.0f;

    if (m->kind == K_LAWN)
    {
        const f32 s = (f32)g->cellW / 100.0f;          /* body 100px -> one cell wide */
        const f32 bx = sx - 50.0f * s, by = base - 80.0f * s + jig * s;
        if (!s_t[T_BODY] && !s_t[T_TBODY])
        { GRRLIB_Rectangle(sx - 18, base - 22, 36, 22, 0xC0302AFF, true); return; }
        Part(s_t[T_WHEEL_B], bx, by, 50, 56, s, spin);
        Part(s_t[T_WHEEL_A], bx, by, 14, 56, s, spin);
        if (s_tricked && s_t[T_TBODY])
        {
            Part(s_t[T_TBODY],   bx, by, 0, 6, s, 0);
            Part(s_t[T_TENGINE], bx, by, 44, -4 - jig, s, 0);
            Part(s_t[T_TPIPES],  bx, by, 56, 56, s, 0);
            Part(s_t[T_DICE],    bx, by, 8, 50, s, 0);
        }
        else
        {
            Part(s_t[T_BODY],   bx, by, 0, 0, s, 0);
            Part(s_t[T_ENGINE], bx, by, 52, 18 - jig, s, 0);
        }
    }
    else if (m->kind == K_POOL)
    {
        const f32 s = (f32)g->cellW / 72.0f;
        const f32 bx = sx - 30.0f * s, by = base - 60.0f * s;
        if (!s_t[T_PBODY])
        { GRRLIB_Rectangle(sx - 16, base - 28, 32, 28, 0x2F5FBFFF, true); return; }
        Part(s_t[T_PWHEEL],  bx, by, 10, 30, s, spin);
        Part(s_t[T_PBODY],   bx, by, 0, 0, s, 0);
        Part(s_t[T_PFUNNEL], bx, by, 20, -5 - jig, s, 0);
    }
    else
    {
        const f32 s = (f32)g->cellW / 80.0f;
        const f32 bx = sx - 34.0f * s, by = base - 62.0f * s;
        if (!s_t[T_RBODY])
        { GRRLIB_Rectangle(sx - 18, base - 30, 36, 30, 0xB0B0B8FF, true); return; }
        int bf = m->running ? (int)(m->x * 12.0f) % 5 : 0;
        if (bf < 0) bf += 5;
        Part(s_t[T_BRUSH + bf], bx, by, 0, 34, s, 0);
        Part(s_t[T_RBODY],      bx, by, 0, 0, s, 0);
        Part(s_t[T_RCLOCK],     bx, by, 26, 18, s, 0);
    }
}

extern "C" void Mower_Draw(const GameContext* game)
{
    if (!game || !s_ready) return;
    if (game->state != GAME_STATE_PLAYING && game->state != GAME_STATE_GAME_OVER) return;
    for (int r = 0; r < game->rowCount && r < GRID_ROWS; ++r)
        if (s_m[r].present) DrawOne(game, r, &s_m[r]);
}

extern "C" void Mower_Free(void)
{
    for (int k = 0; k < K_COUNT; ++k) FreeKind(k);
    s_ready = false;
}
