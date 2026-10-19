/**
 * modes.cpp - Puzzle menu + Vasebreaker.
 *
 * SELF-CONTAINED: its own tiny simulation and renderer, so it does not touch
 * GameContext / types / the adventure code. Only needs InputState (input.h),
 * the Render_* helpers from render.h (re-declared below) and constants.h.
 *
 * Hooks:
 *   menu.cpp  title case 2 (Puzzle):  Modes_Open();
 *   main.cpp  in the loop, before the Menu_IsTitle() branch:
 *               if (Modes_Active()) { Modes_Update(&input, WPAD_ButtonsDown(0)); }
 *               else if (Menu_IsTitle(&menu)) { ...existing... } else { ...existing... }
 *             and for drawing:
 *               if (Modes_Active()) { Modes_Render(&input); Render_DrawReticle(&input); }
 *               else { Render_Frame(...); Menu_Render(...); ... }
 *               Render_Present();
 *   render.cpp Render_Shutdown: Modes_Free();
 *
 * Vasebreaker rules (simplified from the original):
 *   - Point and click (A/B) a vase to smash it. Green leaf vase = a plant seed,
 *     grey face vase = a zombie, "?" vase = either.
 *   - A smashed plant vase adds a seed to your seed bar: click the seed, then an
 *     empty lawn tile to plant it. Zombies walk left from the vase they came out of.
 *   - Clear every vase and kill every zombie to win; a zombie reaching the house loses.
 *   - Progress (highest level cleared) is saved in sd:/apps/pvz_wii/vase.cfg and
 *     each win pays COINS_PER_WIN coins (if hud.cpp's Coins_Add is present).
 * Zen Garden is listed in the menu as "coming soon" (step 6b).
 *
 * Assets (relative to assets/): vasebreaker/vase_{mystery,plant,zombie}[_inside].png,
 * vasebreaker/shard_{mystery,plant,zombie}_1..9.png, background/every lawn day.png,
 * ui/seedbar_bg.png + ui/seed_frame.png (from step 4, optional),
 * zombies/{basic,conehead,buckhead}/full.png, projectiles/{pea,snow_pea}.png.
 * Anything missing falls back to flat colours.
 */
#include "input.h"
#include "constants.h"
#include "types.h"
#include <grrlib.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <sys/stat.h>

extern "C" {
GRRLIB_texImg* Render_LoadTexture(const char* relPath);
void Render_DrawText(s16 x, s16 y, u32 color, unsigned int size, const char* text);
void Render_DrawTextCentered(s16 cx, s16 y, u32 color, unsigned int size, const char* text);
void Render_DrawPlantPortrait(PlantType type, s16 cx, s16 bottomY, s16 targetH, s16 maxW, u32 frameCount);
void Coins_Add(u32 n);   /* hud.cpp (step 4) */
}

#ifndef SETTINGS_DIR
#define SETTINGS_DIR "sd:/apps/pvz_wii"
#endif
#define VASE_CFG SETTINGS_DIR "/vase.cfg"
#define COINS_PER_WIN 20
#define VASE_LEVELS   5

/* ------------------------------------------------------------------ */
/* Shared helpers                                                       */
/* ------------------------------------------------------------------ */
enum Screen { S_NONE, S_MENU, S_SELECT, S_PLAY };
static Screen s_screen = S_NONE;
static u32    s_frame  = 0;
static int    s_cursor = 0;
static bool   s_assetsLoaded = false;

static GRRLIB_texImg *s_bg, *s_bar, *s_seedFrame;
static GRRLIB_texImg *s_vase[3], *s_vaseIn[3], *s_shard[3][9];
static GRRLIB_texImg *s_zomb[3], *s_pea, *s_snow;

static const int LX = LAWN_MARGIN_X;
static const int CW = CELL_WIDTH_DEFAULT;
static const int CH = CELL_HEIGHT_DEFAULT;
static const int LY = LAWN_MARGIN_Y + ((GRID_ROWS - 5) * CELL_HEIGHT_DEFAULT) / 2;
#define ROWS 5

static void Quad(GRRLIB_texImg* t, f32 x, f32 y, f32 w, f32 h, f32 angle, u32 color)
{
    if (!t || !t->data) return;
    const f32 cx = x + w * 0.5f, cy = y + h * 0.5f;
    const f32 rad = angle * 3.14159265f / 180.0f, c = cosf(rad), s = sinf(rad);
    const f32 lx[4] = { -w * 0.5f, w * 0.5f, w * 0.5f, -w * 0.5f };
    const f32 ly[4] = { -h * 0.5f, -h * 0.5f, h * 0.5f, h * 0.5f };
    guVector q[4];
    for (int i = 0; i < 4; ++i)
    { q[i].x = cx + lx[i] * c - ly[i] * s; q[i].y = cy + lx[i] * s + ly[i] * c; q[i].z = 0; }
    GRRLIB_DrawImgQuad(q, t, color);
}

static void LoadAssets(void)
{
    if (s_assetsLoaded) return;
    s_assetsLoaded = true;
    s_bg        = Render_LoadTexture("assets/background/every lawn day.png");
    s_bar       = Render_LoadTexture("assets/ui/seedbar_bg.png");
    s_seedFrame = Render_LoadTexture("assets/ui/seed_frame.png");
    static const char* kind[3] = { "mystery", "plant", "zombie" };
    char p[96];
    for (int k = 0; k < 3; ++k)
    {
        snprintf(p, sizeof(p), "assets/vasebreaker/vase_%s.png", kind[k]);        s_vase[k]   = Render_LoadTexture(p);
        snprintf(p, sizeof(p), "assets/vasebreaker/vase_%s_inside.png", kind[k]); s_vaseIn[k] = Render_LoadTexture(p);
        for (int i = 0; i < 9; ++i)
        { snprintf(p, sizeof(p), "assets/vasebreaker/shard_%s_%d.png", kind[k], i + 1); s_shard[k][i] = Render_LoadTexture(p); }
    }
    s_zomb[0] = Render_LoadTexture("assets/zombies/basic/full.png");
    s_zomb[1] = Render_LoadTexture("assets/zombies/conehead/full.png");
    s_zomb[2] = Render_LoadTexture("assets/zombies/buckhead/full.png");
    s_pea     = Render_LoadTexture("assets/projectiles/pea.png");
    s_snow    = Render_LoadTexture("assets/projectiles/snow_pea.png");
}

extern "C" void Modes_Free(void)
{
    GRRLIB_texImg** one[] = { &s_bg, &s_bar, &s_seedFrame, &s_pea, &s_snow };
    for (unsigned i = 0; i < sizeof(one) / sizeof(one[0]); ++i)
        if (*one[i]) { GRRLIB_FreeTexture(*one[i]); *one[i] = NULL; }
    for (int k = 0; k < 3; ++k)
    {
        if (s_vase[k])   { GRRLIB_FreeTexture(s_vase[k]);   s_vase[k] = NULL; }
        if (s_vaseIn[k]) { GRRLIB_FreeTexture(s_vaseIn[k]); s_vaseIn[k] = NULL; }
        if (s_zomb[k])   { GRRLIB_FreeTexture(s_zomb[k]);   s_zomb[k] = NULL; }
        for (int i = 0; i < 9; ++i)
            if (s_shard[k][i]) { GRRLIB_FreeTexture(s_shard[k][i]); s_shard[k][i] = NULL; }
    }
    s_assetsLoaded = false;
}

static bool In(f32 px, f32 py, int x, int y, int w, int h)
{ return px >= x && px < x + w && py >= y && py < y + h; }

/* ------------------------------------------------------------------ */
/* Progress                                                            */
/* ------------------------------------------------------------------ */
static int s_cleared = 0;           /* levels cleared so far (0..VASE_LEVELS) */
static bool s_progLoaded = false;

static void LoadProgress(void)
{
    if (s_progLoaded) return;
    s_progLoaded = true;
    FILE* f = fopen(VASE_CFG, "r");
    if (!f) return;
    int v = 0;
    if (fscanf(f, "CLEARED=%d", &v) == 1 && v >= 0 && v <= VASE_LEVELS) s_cleared = v;
    fclose(f);
}

static void SaveProgress(void)
{
    mkdir(SETTINGS_DIR, 0777);
    FILE* f = fopen(VASE_CFG, "w");
    if (!f) return;
    fprintf(f, "CLEARED=%d\n", s_cleared);
    fclose(f);
}

/* ------------------------------------------------------------------ */
/* Vasebreaker data                                                    */
/* ------------------------------------------------------------------ */
enum PT { PT_PEA, PT_SNOW, PT_REPEATER, PT_WALL, PT_CHERRY, PT_SQUASH, PT_MINE, PT_COUNT };
static const PlantType kPlantType[PT_COUNT] = {
    PLANT_PEASHOOTER, PLANT_SNOW_PEA, PLANT_REPEATER, PLANT_WALLNUT,
    PLANT_CHERRY_BOMB, PLANT_SQUASH, PLANT_POTATO_MINE };
static const char* kPlantName[PT_COUNT] = {
    "Peashooter", "Snow Pea", "Repeater", "Wall-nut", "Cherry Bomb", "Squash", "Potato Mine" };
static const s16 kPlantHp[PT_COUNT] = { 300, 300, 300, 4000, 300, 300, 300 };

enum ZT { ZT_BASIC, ZT_CONE, ZT_BUCKET, ZT_COUNT };
static const char* kZombName[ZT_COUNT] = { "Zombie", "Conehead Zombie", "Buckethead Zombie" };
static const s16   kZombHp[ZT_COUNT]   = { 200, 560, 1100 };
static const f32   kZombScale[ZT_COUNT] = { 0.30f, 0.31f, 0.30f };

enum VK { VK_MYSTERY = 0, VK_PLANT = 1, VK_ZOMBIE = 2 };

struct Vase { bool alive; u8 col, row, kind, content; u8 contentIsZombie; s8 breakT; };
struct Plant { bool a; u8 type, col, row; s16 hp; u16 timer; };
struct Zomb  { bool a; u8 type, row; f32 x; s16 hp; u16 slow; f32 pop; };
struct Pea   { bool a; u8 row; f32 x; bool snow; };
struct Shard { bool a; u8 kind, idx; f32 x, y, vx, vy, rot, vr; u16 life; };
struct Boom  { bool a; f32 x, y; u8 t; };

#define MAXV 24
#define MAXP 40
#define MAXZ 24
#define MAXPEA 48
#define MAXSH 96
#define MAXSEED 16

static Vase  s_v[MAXV];   static int s_nv;
static Plant s_p[MAXP];
static Zomb  s_z[MAXZ];
static Pea   s_pea_[MAXPEA];
static Shard s_sh[MAXSH];
static Boom  s_boom[8];
static u8    s_seed[MAXSEED]; static int s_nseed;
static int   s_held = -1;
static int   s_level = 0;
static enum { R_PLAY, R_WON, R_LOST } s_result = R_PLAY;
static int   s_resultCursor = 0;

/* Level tables: kind, content (PT or ZT), contentIsZombie. kind 0 = mystery. */
struct VDef { u8 kind, content, isZ; };
#define P_(t) { VK_PLANT, (t), 0 }
#define Z_(t) { VK_ZOMBIE, (t), 1 }
#define M_(t) { VK_MYSTERY, (t), 0 }
#define MZ(t) { VK_MYSTERY, (t), 1 }
static const VDef kL1[] = { P_(PT_PEA), P_(PT_PEA), P_(PT_WALL), Z_(ZT_BASIC), Z_(ZT_BASIC), M_(PT_SNOW) };
static const VDef kL2[] = { P_(PT_PEA), P_(PT_SNOW), P_(PT_CHERRY), P_(PT_WALL), Z_(ZT_BASIC), Z_(ZT_BASIC),
                            Z_(ZT_CONE), M_(PT_REPEATER), MZ(ZT_BASIC) };
static const VDef kL3[] = { P_(PT_PEA), P_(PT_PEA), P_(PT_SNOW), P_(PT_WALL), P_(PT_MINE), P_(PT_SQUASH),
                            Z_(ZT_BASIC), Z_(ZT_CONE), Z_(ZT_CONE), M_(PT_CHERRY), MZ(ZT_CONE), M_(PT_REPEATER) };
static const VDef kL4[] = { P_(PT_PEA), P_(PT_REPEATER), P_(PT_SNOW), P_(PT_WALL), P_(PT_WALL), P_(PT_CHERRY),
                            P_(PT_SQUASH), Z_(ZT_CONE), Z_(ZT_BUCKET), Z_(ZT_BASIC), Z_(ZT_BASIC),
                            M_(PT_PEA), MZ(ZT_BUCKET), M_(PT_MINE) };
static const VDef kL5[] = { P_(PT_REPEATER), P_(PT_REPEATER), P_(PT_SNOW), P_(PT_CHERRY), P_(PT_CHERRY),
                            P_(PT_WALL), P_(PT_SQUASH), P_(PT_MINE), Z_(ZT_BUCKET), Z_(ZT_BUCKET),
                            Z_(ZT_CONE), Z_(ZT_CONE), Z_(ZT_BASIC), M_(PT_SNOW), MZ(ZT_BUCKET), MZ(ZT_CONE), M_(PT_PEA) };
static const VDef* kLevel[VASE_LEVELS] = { kL1, kL2, kL3, kL4, kL5 };
static const int   kLevelN[VASE_LEVELS] = {
    (int)(sizeof(kL1) / sizeof(kL1[0])), (int)(sizeof(kL2) / sizeof(kL2[0])), (int)(sizeof(kL3) / sizeof(kL3[0])),
    (int)(sizeof(kL4) / sizeof(kL4[0])), (int)(sizeof(kL5) / sizeof(kL5[0])) };

static u32 s_rng;
static u32 Rn(void) { s_rng = s_rng * 1664525u + 1013904223u; return s_rng >> 8; }
static f32 Rf(f32 lo, f32 hi) { return lo + (hi - lo) * ((Rn() & 0xFFFF) / 65535.0f); }

static void StartLevel(int level)
{
    s_level = level;
    memset(s_v, 0, sizeof(s_v)); memset(s_p, 0, sizeof(s_p)); memset(s_z, 0, sizeof(s_z));
    memset(s_pea_, 0, sizeof(s_pea_)); memset(s_sh, 0, sizeof(s_sh)); memset(s_boom, 0, sizeof(s_boom));
    s_nseed = 0; s_held = -1; s_result = R_PLAY; s_resultCursor = 0;
    s_rng = 12345u + level * 7919u;
    /* Shuffle the 4x5 block of cells (cols 5..8) and drop the vases in. */
    int cells[20];
    for (int i = 0; i < 20; ++i) cells[i] = i;
    for (int i = 19; i > 0; --i) { int j = Rn() % (i + 1); int t = cells[i]; cells[i] = cells[j]; cells[j] = t; }
    s_nv = kLevelN[level];
    for (int i = 0; i < s_nv; ++i)
    {
        const VDef* d = &kLevel[level][i];
        Vase* v = &s_v[i];
        v->alive = true; v->col = (u8)(5 + cells[i] % 4); v->row = (u8)(cells[i] / 4);
        v->kind = d->kind; v->content = d->content; v->contentIsZombie = d->isZ; v->breakT = -1;
    }
}

/* ------------------------------------------------------------------ */
/* Simulation                                                          */
/* ------------------------------------------------------------------ */
static void SpawnZombie(int type, int row, f32 x)
{
    for (int i = 0; i < MAXZ; ++i)
        if (!s_z[i].a) { s_z[i].a = true; s_z[i].type = (u8)type; s_z[i].row = (u8)row; s_z[i].x = x;
                         s_z[i].hp = kZombHp[type]; s_z[i].slow = 0; s_z[i].pop = 1.0f; return; }
}

static void SpawnShards(int kind, f32 cx, f32 cy)
{
    for (int n = 0; n < 9; ++n)
        for (int i = 0; i < MAXSH; ++i)
            if (!s_sh[i].a)
            {
                Shard* s = &s_sh[i];
                s->a = true; s->kind = (u8)kind; s->idx = (u8)n; s->x = cx; s->y = cy;
                s->vx = Rf(-3.0f, 3.0f); s->vy = Rf(-5.0f, -1.5f); s->rot = Rf(0, 360); s->vr = Rf(-12, 12);
                s->life = 45;
                break;
            }
}

static void BreakVase(Vase* v)
{
    v->alive = false; v->breakT = 14;
    SpawnShards(v->kind, LX + v->col * CW + CW * 0.5f, LY + v->row * CH + CH * 0.5f);
    if (v->contentIsZombie) SpawnZombie(v->content, v->row, v->col + 0.5f);
    else if (s_nseed < MAXSEED) s_seed[s_nseed++] = v->content;
}

static bool VaseAt(int col, int row)
{
    for (int i = 0; i < s_nv; ++i) if (s_v[i].alive && s_v[i].col == col && s_v[i].row == row) return true;
    return false;
}
static Plant* PlantAt(int col, int row)
{
    for (int i = 0; i < MAXP; ++i) if (s_p[i].a && s_p[i].col == col && s_p[i].row == row) return &s_p[i];
    return NULL;
}
static bool ZombieInRow(int row, f32 minX)
{
    for (int i = 0; i < MAXZ; ++i) if (s_z[i].a && s_z[i].row == row && s_z[i].x > minX) return true;
    return false;
}
static void Hurt(Zomb* z, int dmg) { z->hp -= (s16)dmg; if (z->hp <= 0) z->a = false; }
static void AddBoom(f32 x, f32 y)
{ for (int i = 0; i < 8; ++i) if (!s_boom[i].a) { s_boom[i].a = true; s_boom[i].x = x; s_boom[i].y = y; s_boom[i].t = 0; return; } }
static void FirePea(int row, f32 x, bool snow)
{ for (int i = 0; i < MAXPEA; ++i) if (!s_pea_[i].a) { s_pea_[i].a = true; s_pea_[i].row = (u8)row; s_pea_[i].x = x; s_pea_[i].snow = snow; return; } }

static void PlacePlant(int type, int col, int row)
{
    for (int i = 0; i < MAXP; ++i)
        if (!s_p[i].a) { s_p[i].a = true; s_p[i].type = (u8)type; s_p[i].col = (u8)col; s_p[i].row = (u8)row;
                         s_p[i].hp = kPlantHp[type]; s_p[i].timer = (type == PT_MINE) ? 240 : 30; return; }
}

static void SimStep(void)
{
    /* plants */
    for (int i = 0; i < MAXP; ++i)
    {
        Plant* p = &s_p[i]; if (!p->a) continue;
        const f32 cx = p->col + 0.5f;
        if (p->type == PT_PEA || p->type == PT_SNOW || p->type == PT_REPEATER)
        {
            if (p->timer > 0) p->timer--;
            if (p->timer == 0 && ZombieInRow(p->row, cx))
            {
                FirePea(p->row, cx + 0.3f, p->type == PT_SNOW);
                if (p->type == PT_REPEATER) FirePea(p->row, cx - 0.1f, false);
                p->timer = 86; /* ~1.425 s */
            }
        }
        else if (p->type == PT_CHERRY)
        {
            if (p->timer > 0) { p->timer--; continue; }
            for (int j = 0; j < MAXZ; ++j)
                if (s_z[j].a && abs((int)s_z[j].row - (int)p->row) <= 1 && fabsf(s_z[j].x - cx) < 1.6f) Hurt(&s_z[j], 1800);
            AddBoom(LX + cx * CW, LY + p->row * CH + CH * 0.5f);
            p->a = false;
        }
        else if (p->type == PT_SQUASH)
        {
            for (int j = 0; j < MAXZ; ++j)
                if (s_z[j].a && s_z[j].row == p->row && s_z[j].x > cx - 0.3f && s_z[j].x < cx + 1.8f)
                { Hurt(&s_z[j], 1800); AddBoom(LX + s_z[j].x * CW, LY + p->row * CH + CH * 0.6f); p->a = false; break; }
        }
        else if (p->type == PT_MINE)
        {
            if (p->timer > 0) { p->timer--; continue; }
            for (int j = 0; j < MAXZ; ++j)
                if (s_z[j].a && s_z[j].row == p->row && fabsf(s_z[j].x - cx) < 0.5f)
                {
                    for (int k = 0; k < MAXZ; ++k)
                        if (s_z[k].a && s_z[k].row == p->row && fabsf(s_z[k].x - cx) < 0.9f) Hurt(&s_z[k], 1800);
                    AddBoom(LX + cx * CW, LY + p->row * CH + CH * 0.6f); p->a = false; break;
                }
        }
    }
    /* peas */
    for (int i = 0; i < MAXPEA; ++i)
    {
        Pea* q = &s_pea_[i]; if (!q->a) continue;
        q->x += 0.09f;
        if (q->x > 9.5f) { q->a = false; continue; }
        for (int j = 0; j < MAXZ; ++j)
            if (s_z[j].a && s_z[j].row == q->row && s_z[j].x - 0.25f < q->x && s_z[j].x + 0.25f > q->x)
            { Hurt(&s_z[j], 20); if (q->snow) s_z[j].slow = 240; q->a = false; break; }
    }
    /* zombies */
    for (int i = 0; i < MAXZ; ++i)
    {
        Zomb* z = &s_z[i]; if (!z->a) continue;
        if (z->pop > 0) z->pop -= 0.05f;
        if (z->slow > 0) z->slow--;
        Plant* eat = NULL;
        for (int k = 0; k < MAXP; ++k)
            if (s_p[k].a && s_p[k].row == z->row && s_p[k].type != PT_MINE) {
                f32 pc = s_p[k].col + 0.5f;
                if (pc < z->x && pc > z->x - 0.6f) { eat = &s_p[k]; break; } }
        if (eat) { eat->hp -= (z->slow ? 1 : 2) ; if (eat->hp <= 0) eat->a = false; } /* ~100/60 dps */
        else z->x -= 0.0040f * (z->slow ? 0.5f : 1.0f);
        if (z->x < 0.1f) { s_result = R_LOST; s_resultCursor = 0; }
    }
    /* win check */
    if (s_result == R_PLAY)
    {
        bool any = false;
        for (int i = 0; i < s_nv; ++i) if (s_v[i].alive) any = true;
        for (int i = 0; i < MAXZ; ++i) if (s_z[i].a) any = true;
        if (!any)
        {
            s_result = R_WON; s_resultCursor = 0;
            if (s_level + 1 > s_cleared) { s_cleared = s_level + 1; SaveProgress(); }
            Coins_Add(COINS_PER_WIN);
        }
    }
    /* shards + booms */
    for (int i = 0; i < MAXSH; ++i)
        if (s_sh[i].a) { Shard* s = &s_sh[i]; s->x += s->vx; s->y += s->vy; s->vy += 0.3f; s->rot += s->vr;
                         if (s->life == 0 || --s->life == 0) s->a = false; }
    for (int i = 0; i < 8; ++i) if (s_boom[i].a && ++s_boom[i].t > 18) s_boom[i].a = false;
    for (int i = 0; i < s_nv; ++i) if (s_v[i].breakT > 0) s_v[i].breakT--;
}

/* ------------------------------------------------------------------ */
/* Input                                                               */
/* ------------------------------------------------------------------ */
static const int SEED_X0 = 76, SEED_Y = 8, SEED_W = 52, SEED_H = 56;

static void ClickPlay(const InputState* in)
{
    const f32 px = in->cursor.x, py = in->cursor.y;
    /* seed bar */
    for (int i = 0; i < s_nseed && i < 9; ++i)
        if (In(px, py, SEED_X0 + i * (SEED_W + 4), SEED_Y, SEED_W, SEED_H))
        { s_held = (s_held == i) ? -1 : i; return; }
    int col = (int)((px - LX) / CW), row = (int)((py - LY) / CH);
    if (px < LX || py < LY || col < 0 || col >= GRID_COLS || row < 0 || row >= ROWS) { s_held = -1; return; }
    if (s_held >= 0)
    {
        if (!VaseAt(col, row) && !PlantAt(col, row))
        {
            PlacePlant(s_seed[s_held], col, row);
            for (int i = s_held; i < s_nseed - 1; ++i) s_seed[i] = s_seed[i + 1];
            s_nseed--; s_held = -1;
        }
        return;
    }
    for (int i = 0; i < s_nv; ++i)
        if (s_v[i].alive && s_v[i].col == col && s_v[i].row == row) { BreakVase(&s_v[i]); return; }
}

/* ------------------------------------------------------------------ */
/* Menus                                                               */
/* ------------------------------------------------------------------ */
static const char* kMenu[3] = { "Vasebreaker", "Zen Garden  (coming soon)", "Back" };

static int MenuHit(const InputState* in, int n, int x0, int y0, int w, int h, int gap)
{
    if (!in->cursorValid) return -1;
    for (int i = 0; i < n; ++i)
        if (In(in->cursor.x, in->cursor.y, x0, y0 + i * (h + gap), w, h)) return i;
    return -1;
}

extern "C" void Modes_Open(void)
{
    LoadAssets(); LoadProgress();
    s_screen = S_MENU; s_cursor = 0;
}
extern "C" bool Modes_Active(void) { return s_screen != S_NONE; }

extern "C" void Modes_Update(const InputState* in, unsigned wpadDown)
{
    s_frame++;
    const bool home = (wpadDown & 0x0080) != 0;   /* WPAD_BUTTON_HOME */
    const bool click = in->placePressed;
    if (home) { s_screen = (s_screen == S_PLAY) ? S_SELECT : S_NONE; if (s_screen == S_SELECT) s_cursor = s_level; return; }

    if (s_screen == S_MENU)
    {
        int h = MenuHit(in, 3, 170, 170, 300, 44, 14);
        if (h >= 0) s_cursor = h;
        s_cursor += in->tileMoveY; if (s_cursor < 0) s_cursor = 2; if (s_cursor > 2) s_cursor = 0;
        if (click)
        {
            if (s_cursor == 0) { s_screen = S_SELECT; s_cursor = (s_cleared < VASE_LEVELS) ? s_cleared : VASE_LEVELS - 1; }
            else if (s_cursor == 2) s_screen = S_NONE;
        }
    }
    else if (s_screen == S_SELECT)
    {
        int h = MenuHit(in, VASE_LEVELS + 1, 190, 130, 260, 36, 8);
        if (h >= 0) s_cursor = h;
        s_cursor += in->tileMoveY; if (s_cursor < 0) s_cursor = VASE_LEVELS; if (s_cursor > VASE_LEVELS) s_cursor = 0;
        if (click)
        {
            if (s_cursor == VASE_LEVELS) { s_screen = S_MENU; s_cursor = 0; }
            else if (s_cursor <= s_cleared) { StartLevel(s_cursor); s_screen = S_PLAY; }
        }
    }
    else if (s_screen == S_PLAY)
    {
        if (s_result == R_PLAY)
        {
            if (click && in->cursorValid) ClickPlay(in);
            SimStep();
        }
        else
        {
            int h = MenuHit(in, 2, 200, 250, 240, 40, 10);
            if (h >= 0) s_resultCursor = h;
            s_resultCursor += in->tileMoveY; if (s_resultCursor < 0) s_resultCursor = 1; if (s_resultCursor > 1) s_resultCursor = 0;
            if (click)
            {
                if (s_resultCursor == 1) { s_screen = S_SELECT; s_cursor = (s_cleared < VASE_LEVELS) ? s_cleared : VASE_LEVELS - 1; }
                else if (s_result == R_WON && s_level + 1 < VASE_LEVELS) StartLevel(s_level + 1);
                else StartLevel(s_level);
            }
        }
    }
}

/* ------------------------------------------------------------------ */
/* Rendering                                                           */
/* ------------------------------------------------------------------ */
static void Button(int x, int y, int w, int h, bool sel, bool enabled, const char* text)
{
    GRRLIB_Rectangle(x - 3, y - 3, w + 6, h + 6, sel ? 0xFFE27AFF : 0x2E1608FF, true);
    GRRLIB_Rectangle(x, y, w, h, enabled ? (sel ? 0xC97A34FF : 0x8A4A1EFF) : 0x555555FF, true);
    Render_DrawTextCentered(x + w / 2, y + h / 2 - 9, enabled ? 0xFFFFFFFF : 0xBBBBBBFF, 20, text);
}

static void DrawBackdrop(void)
{
    if (s_bg && s_bg->data) Quad(s_bg, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0, 0xFFFFFFFF);
    else GRRLIB_Rectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0x4A8C3AFF, true);
}

static void DrawVase(const Vase* v)
{
    const f32 w = 52, h = 65;
    const f32 x = LX + v->col * CW + (CW - w) * 0.5f, y = LY + v->row * CH + CH - h + 4;
    GRRLIB_texImg* t = s_vase[v->kind];
    if (v->alive)
    {
        if (t && t->data) Quad(t, x, y, w, h, 0, 0xFFFFFFFF);
        else GRRLIB_Rectangle(x + 8, y + 8, w - 16, h - 8, v->kind == 1 ? 0x2E8B2EFF : (v->kind == 2 ? 0x777777FF : 0x8A4A1EFF), true);
    }
    else if (v->breakT > 0)
    {
        GRRLIB_texImg* in = s_vaseIn[v->kind];
        u32 a = (u32)(255 * v->breakT / 14);
        if (in && in->data) Quad(in, x, y, w, h, 0, 0xFFFFFF00 | a);
    }
}

static void DrawZombie(const Zomb* z)
{
    GRRLIB_texImg* t = s_zomb[z->type];
    const f32 sc = kZombScale[z->type];
    const f32 bob = sinf((f32)s_frame * 0.12f + z->row) * 1.5f;
    const f32 feetX = LX + z->x * CW, feetY = LY + (z->row + 1) * CH - 4 + bob;
    if (t && t->data)
    {
        f32 w = t->w * sc, h = t->h * sc;
        u32 tint = z->slow ? 0xA8D8FFFF : 0xFFFFFFFF;
        Quad(t, feetX - w * 0.5f, feetY - h + (z->pop > 0 ? z->pop * 30 : 0), w, h, 0, tint);
    }
    else GRRLIB_Rectangle(feetX - 10, feetY - 40, 20, 40, 0x6B8E6BFF, true);
}

static void DrawPlay(const InputState* in)
{
    DrawBackdrop();
    /* vases + plants + zombies sorted by row so overlaps look right */
    for (int r = 0; r < ROWS; ++r)
    {
        for (int i = 0; i < s_nv; ++i) if (s_v[i].row == r) DrawVase(&s_v[i]);
        for (int i = 0; i < MAXP; ++i)
            if (s_p[i].a && s_p[i].row == r)
            {
                const Plant* p = &s_p[i];
                Render_DrawPlantPortrait(kPlantType[p->type], (s16)(LX + p->col * CW + CW / 2),
                                         (s16)(LY + (r + 1) * CH - 2), 58, 56, s_frame);
                if (p->type == PT_MINE && p->timer > 0)
                    GRRLIB_Rectangle(LX + p->col * CW + 8, LY + (r + 1) * CH - 10, CW - 16, 4, 0x000000A0, true);
            }
        for (int i = 0; i < MAXZ; ++i) if (s_z[i].a && s_z[i].row == r) DrawZombie(&s_z[i]);
    }
    for (int i = 0; i < MAXPEA; ++i)
        if (s_pea_[i].a)
        {
            f32 x = LX + s_pea_[i].x * CW, y = LY + s_pea_[i].row * CH + CH * 0.35f;
            GRRLIB_texImg* t = s_pea_[i].snow ? s_snow : s_pea;
            if (t && t->data) Quad(t, x - 10, y - 10, 20, 20, 0, 0xFFFFFFFF);
            else GRRLIB_Circle(x, y, 5, s_pea_[i].snow ? 0x66CCFFFF : 0x4CAF30FF, true);
        }
    for (int i = 0; i < 8; ++i)
        if (s_boom[i].a)
        {
            f32 t = s_boom[i].t / 18.0f;
            GRRLIB_Circle(s_boom[i].x, s_boom[i].y, 20 + 50 * t, (0xFFA02000u | (u32)(200 * (1 - t))), true);
        }
    for (int i = 0; i < MAXSH; ++i)
        if (s_sh[i].a)
        {
            GRRLIB_texImg* t = s_shard[s_sh[i].kind][s_sh[i].idx];
            if (t && t->data)
            {
                f32 w = t->w * 0.8f, h = t->h * 0.8f;
                Quad(t, s_sh[i].x - w / 2, s_sh[i].y - h / 2, w, h, s_sh[i].rot,
                     0xFFFFFF00 | (u32)(255 * (s_sh[i].life > 15 ? 15 : s_sh[i].life) / 15));
            }
        }

    /* seed bar */
    if (s_bar && s_bar->data) Quad(s_bar, 0, 0, SCREEN_WIDTH, 72, 0, 0xFFFFFFFF);
    else GRRLIB_Rectangle(0, 0, SCREEN_WIDTH, 72, 0x6B3410FF, true);
    char buf[48];
    snprintf(buf, sizeof(buf), "Level %d", s_level + 1);
    Render_DrawTextCentered(34, 22, 0x3A2A10FF, 14, buf);
    for (int i = 0; i < s_nseed && i < 9; ++i)
    {
        int x = SEED_X0 + i * (SEED_W + 4);
        if (s_seedFrame && s_seedFrame->data) Quad(s_seedFrame, x, SEED_Y, SEED_W, SEED_H, 0, 0xFFFFFFFF);
        else GRRLIB_Rectangle(x, SEED_Y, SEED_W, SEED_H, 0xEEEEEEFF, true);
        Render_DrawPlantPortrait(kPlantType[s_seed[i]], (s16)(x + SEED_W / 2), (s16)(SEED_Y + 42), 36, 36, s_frame);
        if (i == s_held) GRRLIB_Rectangle(x - 2, SEED_Y - 2, SEED_W + 4, 3, 0xFFE27AFF, true);
    }

    /* held plant follows the pointer; tooltip over vases */
    if (in->cursorValid)
    {
        if (s_held >= 0)
            Render_DrawPlantPortrait(kPlantType[s_seed[s_held]], (s16)in->cursor.x, (s16)(in->cursor.y + 30), 58, 56, s_frame);
        else
        {
            int col = (int)((in->cursor.x - LX) / CW), row = (int)((in->cursor.y - LY) / CH);
            for (int i = 0; i < s_nv; ++i)
                if (s_v[i].alive && s_v[i].col == col && s_v[i].row == row && s_v[i].kind != VK_MYSTERY)
                {
                    const char* n = s_v[i].contentIsZombie ? kZombName[s_v[i].content] : kPlantName[s_v[i].content];
                    GRRLIB_Rectangle(in->cursor.x + 12, in->cursor.y + 8, 150, 22, 0x000000B0, true);
                    Render_DrawText((s16)(in->cursor.x + 18), (s16)(in->cursor.y + 12), 0xFFFFFFFF, 13, n);
                }
        }
    }

    if (s_result != R_PLAY)
    {
        GRRLIB_Rectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0x000000A0, true);
        Render_DrawTextCentered(320, 170, s_result == R_WON ? 0xFFE27AFF : 0xFF5050FF, 30,
                                s_result == R_WON ? "Level cleared!" : "The zombies got in!");
        if (s_result == R_WON) { snprintf(buf, sizeof(buf), "+%d coins", COINS_PER_WIN); Render_DrawTextCentered(320, 208, 0xFFFFFFFF, 16, buf); }
        const char* a = (s_result == R_WON && s_level + 1 < VASE_LEVELS) ? "Next level" : (s_result == R_WON ? "Play again" : "Retry");
        Button(200, 250, 240, 40, s_resultCursor == 0, true, a);
        Button(200, 300, 240, 40, s_resultCursor == 1, true, "Level select");
    }
}

extern "C" void Modes_Render(const InputState* in)
{
    GRRLIB_FillScreen(0x000000FF);
    if (s_screen == S_PLAY) { DrawPlay(in); return; }
    DrawBackdrop();
    GRRLIB_Rectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0x00000070, true);
    if (s_screen == S_MENU)
    {
        Render_DrawTextCentered(320, 90, 0xFFE27AFF, 32, "Puzzle");
        for (int i = 0; i < 3; ++i) Button(170, 170 + i * 58, 300, 44, s_cursor == i, i != 1, kMenu[i]);
    }
    else if (s_screen == S_SELECT)
    {
        Render_DrawTextCentered(320, 70, 0xFFE27AFF, 30, "Vasebreaker");
        char buf[40];
        for (int i = 0; i < VASE_LEVELS; ++i)
        {
            snprintf(buf, sizeof(buf), (i < s_cleared) ? "Level %d  -  cleared" : "Level %d", i + 1);
            Button(190, 130 + i * 44, 260, 36, s_cursor == i, i <= s_cleared, buf);
        }
        Button(190, 130 + VASE_LEVELS * 44, 260, 36, s_cursor == VASE_LEVELS, true, "Back");
    }
}
