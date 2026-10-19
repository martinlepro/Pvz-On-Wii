/**
 * hud.cpp - In-game HUD art + two small mechanics that go with it:
 *   * seed bar background, seed-packet frame, sun counter on the scroll
 *   * seed-packet recharge (cooldown) with a dimming overlay
 *   * Shovel slot + working Shovel tool (only if the shovel item was earned)
 *   * Coin bank (persistent coins, +COIN_LEVEL_BONUS per level cleared)
 *
 * SELF-CONTAINED: no header or struct changes; all state is file-local.
 *
 * Needs the derived textures from tools/make_hud_assets.py (run it once on
 * your assets/ folder): ui/seedbar_bg.png, ui/seed_frame.png,
 * ui/shovel_slot_small.png, ui/coinbank_small.png. Plus items/shovel.png.
 * Every draw function returns false / does nothing when its art is missing,
 * and render.cpp then keeps the old flat-colour look.
 *
 * Hooks:
 *   render.cpp  Render_Init: Hud_Load();  Render_Shutdown: Hud_Free();
 *               DrawSeedBar: Hud_DrawBarBg / Hud_DrawSunCount /
 *                            Hud_DrawPacketFrame / Hud_DrawCooldown / Hud_DrawExtras
 *               Render_Frame (after DrawHeldSeed): Hud_DrawHeldShovel
 *   game.cpp    Game_Update (PLAYING):      Hud_Update(game);
 *               Game_HandleInput (PLAYING): if (Hud_HandleInput(game, input)) return;
 */
#include "game.h"
#include "input.h"
#include "constants.h"
#include "plant.h"
#include "minigame.h"
#include "particle.h"
#include <grrlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>

extern "C" {
GRRLIB_texImg* Render_LoadTexture(const char* relPath);
void Render_DrawTextCentered(s16 cx, s16 y, u32 color, unsigned int size, const char* text);
bool Items_HasShovel(void);   /* items.cpp (step 1) */
}

#ifndef SETTINGS_DIR
#define SETTINGS_DIR "sd:/apps/pvz_wii"
#endif
#define COINS_PATH SETTINGS_DIR "/coins.cfg"

#define COIN_LEVEL_BONUS   25       /* placeholder rule: coins per level cleared */
#define HUD_COOLDOWNS      1        /* 0 = packets never recharge-lock */

static GRRLIB_texImg *s_bar, *s_frame, *s_shovelSlot, *s_coinBank, *s_shovel;
static bool s_loaded = false;

extern "C" void Hud_Load(void)
{
    s_bar        = Render_LoadTexture("assets/ui/seedbar_bg.png");
    s_frame      = Render_LoadTexture("assets/ui/seed_frame.png");
    s_shovelSlot = Render_LoadTexture("assets/ui/shovel_slot_small.png");
    s_coinBank   = Render_LoadTexture("assets/ui/coinbank_small.png");
    s_shovel     = Render_LoadTexture("assets/items/shovel.png");
    s_loaded = true;
}

extern "C" void Hud_Free(void)
{
    GRRLIB_texImg** all[] = { &s_bar, &s_frame, &s_shovelSlot, &s_coinBank, &s_shovel };
    for (unsigned i = 0; i < sizeof(all) / sizeof(all[0]); ++i)
        if (*all[i]) { GRRLIB_FreeTexture(*all[i]); *all[i] = NULL; }
    s_loaded = false;
}

static void DrawTex(GRRLIB_texImg* t, f32 x, f32 y, f32 sx, f32 sy, f32 angleDeg, u32 color)
{
    if (!t || !t->data) return;
    const f32 hw = t->w * sx * 0.5f, hh = t->h * sy * 0.5f;
    const f32 cx = x + hw, cy = y + hh;
    const f32 rad = angleDeg * 3.14159265f / 180.0f, c = cosf(rad), s = sinf(rad);
    const f32 lx[4] = { -hw, hw, hw, -hw }, ly[4] = { -hh, -hh, hh, hh };
    guVector q[4];
    for (int i = 0; i < 4; ++i)
    { q[i].x = cx + lx[i] * c - ly[i] * s; q[i].y = cy + lx[i] * s + ly[i] * c; q[i].z = 0; }
    GRRLIB_DrawImgQuad(q, t, color);
}

/* ------------------------------------------------------------------ */
/* Coins (persistent)                                                  */
/* ------------------------------------------------------------------ */
static u32  s_coins = 0;
static bool s_coinsLoaded = false;

static void LoadCoins(void)
{
    if (s_coinsLoaded) return;
    s_coinsLoaded = true;
    FILE* f = fopen(COINS_PATH, "r");
    if (!f) return;
    unsigned v = 0;
    if (fscanf(f, "COINS=%u", &v) == 1) s_coins = v;
    fclose(f);
}

static void SaveCoins(void)
{
    mkdir(SETTINGS_DIR, 0777);
    FILE* f = fopen(COINS_PATH, "w");
    if (!f) return;
    fprintf(f, "COINS=%u\n", (unsigned)s_coins);
    fclose(f);
}

extern "C" u32  Coins_Get(void) { LoadCoins(); return s_coins; }
extern "C" bool Coins_Spend(u32 n)
{
    LoadCoins();
    if (s_coins < n) return false;
    s_coins -= n; SaveCoins(); return true;
}
extern "C" void Coins_Add(u32 n)
{
    LoadCoins();
    s_coins += n; if (s_coins > 999999) s_coins = 999999;
    SaveCoins();
}

/* ------------------------------------------------------------------ */
/* Seed recharge                                                       */
/* ------------------------------------------------------------------ */
static u16  s_cd[PLANT_TYPE_COUNT];      /* frames left */
static u16  s_cdMax[PLANT_TYPE_COUNT];
static u8   s_prevCount[PLANT_TYPE_COUNT];
static bool s_synced = false;
static bool s_wonPaid = false;

static u16 RechargeFrames(PlantType t)
{
    const PlantDef* d = Plant_GetDef(t);
    switch (d ? d->recharge : RECHARGE_FAST)
    {
        case RECHARGE_FAST:      return 450;    /* 7.5 s */
        case RECHARGE_MEDIUM:    return 900;    /* 15 s  */
        case RECHARGE_SLOW:      return 1800;   /* 30 s  */
        default:                 return 3000;   /* 50 s  */
    }
}

static bool CooldownsOn(const GameContext* g)
{
    return HUD_COOLDOWNS && !Minigame_IsMinigame((u8)g->currentLevelIndex);
}

static void CountPlants(const GameContext* g, u8 out[PLANT_TYPE_COUNT])
{
    memset(out, 0, PLANT_TYPE_COUNT);
    for (int r = 0; r < g->rowCount && r < GRID_ROWS; ++r)
        for (int c = 0; c < GRID_COLS; ++c)
        {
            PlantType p = g->grid[r][c].plant;
            if (p >= 0 && p < PLANT_TYPE_COUNT && out[p] < 255) out[p]++;
        }
}

/* ------------------------------------------------------------------ */
/* Shovel                                                              */
/* ------------------------------------------------------------------ */
static bool s_shovelHeld = false;

static void ShovelRect(const GameContext* g, int* x, int* y, int* w, int* h)
{
    *w = 56; *h = 56;
    if (g->selectedCount >= SEED_SLOT_COUNT)   /* bar is full: tuck it under the bar */
    { *x = SCREEN_WIDTH - 58; *y = SEED_BAR_HEIGHT + 2; }
    else
    { *x = SCREEN_WIDTH - 62; *y = (SEED_BAR_HEIGHT - 56) / 2; }
}

static bool InRect(f32 px, f32 py, int x, int y, int w, int h)
{
    return px >= x && px < x + w && py >= y && py < y + h;
}

static bool CellAt(const GameContext* g, f32 px, f32 py, int* col, int* row)
{
    if (px < g->lawnX || py < g->lawnY) return false;
    int c = (int)((px - g->lawnX) / g->cellW), r = (int)((py - g->lawnY) / g->cellH);
    if (c < 0 || c >= GRID_COLS || r < 0 || r >= g->rowCount) return false;
    *col = c; *row = r; return true;
}

static bool DigUp(GameContext* g, int col, int row)
{
    GridCell* cell = &g->grid[row][col];
    bool did = false;
    if (cell->plant != PLANT_NONE)      { cell->plant = PLANT_NONE; cell->health = 0; cell->shootTimer = 0; did = true; }
    else if (cell->hasPumpkin)          { cell->hasPumpkin = false; cell->pumpkinHealth = 0; did = true; }
    else if (cell->hasLilyPad)          { cell->hasLilyPad = false; did = true; }
    else if (cell->hasFlowerPot)        { cell->hasFlowerPot = false; did = true; }
    if (did) Particle_SpawnPoof(g->particles, (f32)col + 0.5f, (f32)row + 0.5f);
    return did;
}

extern "C" bool Hud_HandleInput(GameContext* game, const InputState* input)
{
    if (!game || !input || game->state != GAME_STATE_PLAYING) { s_shovelHeld = false; return false; }

    const bool cd = CooldownsOn(game);
    if (!input->cursorValid)
    {
        s_shovelHeld = false;
        /* Pointer-less fallback plants on a single A press: respect recharge. */
        if (cd && input->placePressed && game->selectedSlot >= 0 &&
            game->selectedSlot < (s8)game->selectedCount)
        {
            PlantType t = game->selectedPlants[game->selectedSlot];
            if (t >= 0 && t < PLANT_TYPE_COUNT && s_cd[t] > 0) return true;
        }
        return false;
    }
    if (!input->placePressed) return false;

    const bool hasShovel = Items_HasShovel();
    int sx, sy, sw, sh; ShovelRect(game, &sx, &sy, &sw, &sh);
    const f32 px = input->cursor.x, py = input->cursor.y;

    if (s_shovelHeld)
    {
        int col, row;
        if (!InRect(px, py, sx, sy, sw, sh) && CellAt(game, px, py, &col, &row))
            DigUp(game, col, row);
        s_shovelHeld = false;           /* click anywhere ends the dig (like seeds) */
        return true;
    }
    if (hasShovel && InRect(px, py, sx, sy, sw, sh))
    {
        if (game->isHoldingSeed) Game_CancelHold(game);
        s_shovelHeld = true;
        return true;
    }
    if (cd && !game->isHoldingSeed && input->hoveredSeedSlot >= 0 &&
        input->hoveredSeedSlot < (s8)game->selectedCount)
    {
        PlantType t = game->selectedPlants[input->hoveredSeedSlot];
        if (t >= 0 && t < PLANT_TYPE_COUNT && s_cd[t] > 0) return true;  /* still recharging */
    }
    return false;
}

/* Called once per simulated frame while PLAYING. */
extern "C" void Hud_Update(GameContext* game)
{
    if (!game) return;
    u8 now[PLANT_TYPE_COUNT];
    CountPlants(game, now);

    if (!s_synced)
    {   /* first frame of a level: adopt the board as-is, everything is ready */
        memset(s_cd, 0, sizeof(s_cd));
        memcpy(s_prevCount, now, sizeof(now));
        s_synced = true;
    }
    s_wonPaid = false;

    if (CooldownsOn(game))
    {
        for (int t = 0; t < PLANT_TYPE_COUNT; ++t)
        {
            if (s_cd[t] > 0) s_cd[t]--;
            if (now[t] > s_prevCount[t])    /* a seed of this type was just planted */
            {
                s_cdMax[t] = RechargeFrames((PlantType)t);
                s_cd[t] = s_cdMax[t];
            }
        }
    }
    memcpy(s_prevCount, now, sizeof(now));
}

/** Called from Level_StartPlaying(): fresh recharge state for the new level. */
extern "C" void Hud_Reset(void)
{
    memset(s_cd, 0, sizeof(s_cd));
    memset(s_prevCount, 0, sizeof(s_prevCount));
    s_synced = false; s_shovelHeld = false; s_wonPaid = false;
}

/** Call every frame from the render side: resets per-level state and pays
 *  the level-clear coin bonus once. Cheap; safe when paused. */
static void Poll(const GameContext* game)
{
    if (game->state == GAME_STATE_SELECTING || game->state == GAME_STATE_GAME_OVER)
        s_synced = false;                       /* next PLAYING frame re-syncs */
    if (game->state != GAME_STATE_PLAYING)
        s_shovelHeld = false;
    if (game->state == GAME_STATE_LEVEL_WON)
    {
        if (!s_wonPaid) { s_wonPaid = true; Coins_Add(COIN_LEVEL_BONUS); }
    }
    else if (game->state == GAME_STATE_PLAYING)
        s_wonPaid = false;
}

/* ------------------------------------------------------------------ */
/* Drawing                                                             */
/* ------------------------------------------------------------------ */
extern "C" bool Hud_DrawBarBg(const GameContext* game)
{
    if (game) Poll(game);
    if (!s_bar || !s_bar->data) return false;
    DrawTex(s_bar, 0, 0, 1.0f, 1.0f, 0, 0xFFFFFFFF);
    return true;
}

/* Sun count on the parchment scroll under the sun disc. */
extern "C" void Hud_DrawSunCount(unsigned sun)
{
    char buf[8];
    snprintf(buf, sizeof(buf), "%u", sun);
    Render_DrawTextCentered(32, 51, 0x3A2A10FF, 14, buf);
}

/* Packet card art behind a seed slot. Returns false if art is missing. */
extern "C" bool Hud_DrawPacketFrame(int slotX, int slotY)
{
    if (!s_frame || !s_frame->data) return false;
    DrawTex(s_frame, (f32)slotX, (f32)slotY, 1.0f, 1.0f, 0, 0xFFFFFFFF);
    return true;
}

/* Dimming overlay that drains from the top as the packet recharges. */
extern "C" void Hud_DrawCooldown(int slotX, int slotY, int w, int h, int plantType)
{
    if (plantType < 0 || plantType >= PLANT_TYPE_COUNT) return;
    if (s_cd[plantType] == 0 || s_cdMax[plantType] == 0) return;
    f32 frac = (f32)s_cd[plantType] / (f32)s_cdMax[plantType];
    GRRLIB_Rectangle((f32)slotX, (f32)slotY, (f32)w, (f32)h * frac, 0x000000A8, true);
}

/* Shovel slot + coin bank. */
extern "C" void Hud_DrawExtras(const GameContext* game, const InputState* input)
{
    (void)input;
    /* Coin bank, bottom-right (the dark lower margin of the lawn) */
    if (s_coinBank && s_coinBank->data)
    {
        const int bx = SCREEN_WIDTH - 108, by = SCREEN_HEIGHT - 26;
        DrawTex(s_coinBank, (f32)bx, (f32)by, 1.0f, 1.0f, 0, 0xFFFFFFFF);
        char buf[12];
        snprintf(buf, sizeof(buf), "%u", (unsigned)Coins_Get());
        Render_DrawTextCentered((s16)(bx + 68), (s16)(by + 4), 0xFFE27AFF, 14, buf);
    }
    /* Shovel slot (only once the item has been earned) */
    if (Items_HasShovel() && s_shovelSlot && s_shovelSlot->data)
    {
        int sx, sy, sw, sh; ShovelRect(game, &sx, &sy, &sw, &sh);
        DrawTex(s_shovelSlot, (f32)sx, (f32)sy, 1.0f, 1.0f, 0, 0xFFFFFFFF);
        if (!s_shovelHeld && s_shovel && s_shovel->data)
            DrawTex(s_shovel, (f32)sx + 4, (f32)sy + 4, 0.6f, 0.6f, 0, 0xFFFFFFFF);
    }
}

/* Shovel on the pointer + highlight of the plant it would dig up. */
extern "C" void Hud_DrawHeldShovel(const GameContext* game, const InputState* input)
{
    if (!s_shovelHeld || !game || !input || !input->cursorValid) return;
    int col, row;
    if (CellAt(game, input->cursor.x, input->cursor.y, &col, &row))
    {
        const GridCell* c = &game->grid[row][col];
        if (c->plant != PLANT_NONE || c->hasPumpkin || c->hasLilyPad || c->hasFlowerPot)
            GRRLIB_Rectangle((f32)(game->lawnX + col * game->cellW), (f32)(game->lawnY + row * game->cellH),
                             (f32)game->cellW, (f32)game->cellH, 0xFF404060, true);
    }
    if (s_shovel && s_shovel->data)
        DrawTex(s_shovel, input->cursor.x - 6, input->cursor.y - 44, 0.7f, 0.7f, 0, 0xFFFFFFFF);
}
