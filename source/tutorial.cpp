/**
 * tutorial.cpp - The two guided tutorials of Adventure Mode (levels 1-1 and 1-2).
 *
 * SELF-CONTAINED: no header or struct changes; all state is file-local (same
 * approach as hud.cpp). Nothing is shown on any other level.
 *
 * Hooks:
 *   level.cpp   Level_StartPlaying():  Tutorial_Reset(game);
 *   game.cpp    Game_Update (PLAYING): Tutorial_Update(game);   (before the sun / wave logic)
 *   render.cpp  Render_Frame:          Tutorial_DrawLaneDim(game) after the lawn,
 *                                      Tutorial_Draw(game)        after the HUD.
 *
 * ---- 1-1 (one lane) ------------------------------------------------------
 *   PICK    yellow arrow on the seed packet + "Click on a seed packet to pick it up".
 *           The lawn lane is dark and lights up to its normal colour while a
 *           seed is held; it goes dark again if the seed is put back.
 *   NICE    first plant placed -> "Nicely done!"                  (3 s)
 *   SUN     first sun falls    -> "Click on the falling sun to collect it."
 *   KEEP    sun collected      -> "Keep on collecting sun! You'll need it to
 *                                  grow more plants!"
 *   ENOUGH  100 sun again      -> "Excellent! You've collected enough for your
 *                                  next plant!" (until a seed packet is clicked)
 *   ZOMBIE  1 s later the first zombie arrives -> "Don't let the zombie reach
 *           your house!" (5 s), then the level plays normally.
 *   Until the first plant is down there is no sky sun and no zombie.
 *
 * ---- 1-2 (three lanes) ---------------------------------------------------
 *   Arrow on the sunflower packet + "Sunflowers are an extremely important plant!"
 *   5 s after the 1st sunflower  -> "Try to plant at least 3 of them."
 *   when the 2nd one is planted  -> "The more sunflowers you have, the faster
 *                                    you can grow plants!"
 *
 * Messages sit in the first non-plantable row BELOW the playable lanes
 * (row 3 in 1-1, row 4 in 1-2), centred on the lawn.
 */
#include "game.h"
#include "input.h"
#include "constants.h"
#include "level.h"
#include "survival.h"
#include "minigame.h"
#include "render.h"
#include <grrlib.h>
#include <math.h>
#include <stdio.h>
#include <string.h>

namespace {

enum Step {
    T_OFF = 0,
    /* 1-1 */
    T1_PICK, T1_NICE, T1_SUN, T1_KEEP, T1_ENOUGH, T1_ZOMBIE_WAIT, T1_ZOMBIE_WARN,
    /* 1-2 */
    T2_SUNFLOWER, T2_WAIT_FIRST, T2_TRY3, T2_MORE
};

const int kNiceFrames       = TARGET_FPS * 3;   /* "Nicely done!" before the first sun   */
const int kZombieDelay      = TARGET_FPS * 1;   /* click on packet -> zombie arrives      */
const int kZombieWarnFrames = TARGET_FPS * 5;
const int kTry3Delay        = TARGET_FPS * 5;   /* 1st sunflower -> "plant at least 3"    */
const int kTry3MaxFrames    = TARGET_FPS * 10;
const int kMoreFrames       = TARGET_FPS * 6;
const int kTutorialSunGap   = TARGET_FPS * 5;   /* sky sun every 5 s while 1-1 teaches    */

Step s_step      = T_OFF;
int  s_timer     = 0;          /* frames spent in the current step                */
int  s_prevSun   = 0;
bool s_prevHold  = false;
f32  s_dim       = 1.0f;       /* 1-1 lane darkness, eased 0..1                   */

int CountPlants(const GameContext* g, bool anyType, PlantType type)
{
    int n = 0;
    for (int r = 0; r < (int)g->rowCount && r < GRID_ROWS; ++r)
        for (int c = 0; c < GRID_COLS; ++c)
        {
            PlantType p = g->grid[r][c].plant;
            if (p == PLANT_NONE) continue;
            if (anyType || p == type) ++n;
        }
    return n;
}

int SunflowerSlot(const GameContext* g)
{
    for (int i = 0; i < (int)g->selectedCount && i < SEED_SLOT_COUNT; ++i)
        if (g->selectedPlants[i] == PLANT_SUNFLOWER) return i;
    return -1;
}

void Go(Step s) { s_step = s; s_timer = 0; }

bool IsAdventure(const GameContext* g)
{
    return !Survival_IsSurvival(g->currentLevelIndex) && !Minigame_IsMinigame(g->currentLevelIndex);
}

const char* Message()
{
    switch (s_step)
    {
        case T1_PICK:        return "Click on a seed packet to pick it up";
        case T1_NICE:        return "Nicely done!";
        case T1_SUN:         return "Click on the falling sun to collect it.";
        case T1_KEEP:        return "Keep on collecting sun! You'll need it to grow more plants!";
        case T1_ENOUGH:      return "Excellent! You've collected enough for your next plant!";
        case T1_ZOMBIE_WARN: return "Don't let the zombie reach your house!";
        case T2_SUNFLOWER:   return "Sunflowers are an extremely important plant!";
        case T2_TRY3:        return "Try to plant at least 3 of them.";
        case T2_MORE:        return "The more sunflowers you have, the faster you can grow plants!";
        default:             return NULL;
    }
}

/* ---- drawing helpers --------------------------------------------------- */

/* Upward-pointing arrow, tip at (cx, tipY): black outline, yellow fill. */
void DrawArrowUp(f32 cx, f32 tipY)
{
    const int headH = 24, headHalf = 17, shaftHalf = 8, shaftH = 22;
    for (int pass = 0; pass < 2; ++pass)
    {
        const int o = (pass == 0) ? 3 : 0;                    /* outline first, fill on top */
        const u32 col = (pass == 0) ? 0x000000FF : 0xFFE41AFF;
        const int top = (int)tipY;
        /* head: a triangle drawn as 1px-high strips, widened by the outline */
        for (int k = -o; k < headH + o; ++k)
        {
            int kk = (k < 0) ? 0 : k;
            int hw = (headHalf * (kk + 1)) / headH + o;
            GRRLIB_Rectangle((f32)(cx - hw), (f32)(top + k), (f32)(hw * 2 + 1), 1.0f, col, true);
        }
        /* shaft */
        GRRLIB_Rectangle((f32)(cx - shaftHalf - o), (f32)(top + headH - 1),
                         (f32)((shaftHalf + o) * 2 + 1), (f32)(shaftH + o + 1), col, true);
    }
    /* highlight along the left of the shaft */
    GRRLIB_Rectangle((f32)(cx - shaftHalf + 2), tipY + headH, 3.0f, (f32)(shaftH - 2), 0xFFF59AFF, true);
}

void DrawMessage(const GameContext* g, const char* text)
{
    unsigned size = 18;
    int w = Render_TextWidth(size, text);
    if (w > SCREEN_WIDTH - 40) { size = 15; w = Render_TextWidth(size, text); }
    const int boxW = w + 34, boxH = 34;
    const int cx   = g->lawnX + (GRID_COLS * g->cellW) / 2;
    /* centre of the first non-plantable row under the playable lanes */
    const int cy   = g->lawnY + (int)g->rowCount * g->cellH + g->cellH / 2;
    const int bx   = cx - boxW / 2, by = cy - boxH / 2;
    GRRLIB_Rectangle((f32)(bx - 3), (f32)(by - 3), (f32)(boxW + 6), (f32)(boxH + 6), 0x2E1608FF, true);
    GRRLIB_Rectangle((f32)bx, (f32)by, (f32)boxW, (f32)boxH, 0xF6E7B4FF, true);
    GRRLIB_Rectangle((f32)bx, (f32)by, (f32)boxW, 3.0f, 0xFFF6D6FF, true);
    Render_DrawTextCentered((s16)cx, (s16)(by + (boxH - (int)size) / 2 - 1), 0x3A2A10FF, size, text);
}

} // namespace

/* -------------------------------------------------------------------------- */

extern "C" void Tutorial_Reset(const GameContext* g)
{
    s_step = T_OFF; s_timer = 0; s_dim = 1.0f;
    s_prevSun = g ? (int)g->sun : 0;
    s_prevHold = false;
    if (!g || !IsAdventure(g)) return;
    if (g->currentLevelIndex == 0)      s_step = T1_PICK;
    else if (g->currentLevelIndex == 1) s_step = T2_SUNFLOWER;
}

extern "C" void Tutorial_Update(GameContext* g)
{
    if (!g || s_step == T_OFF) return;
    ++s_timer;

    const bool holding = g->isHoldingSeed;
    const int  sun     = (int)g->sun;
    LevelWave* wave    = &g->waves[g->currentWave];

    switch (s_step)
    {
        /* ----------------------------- 1-1 ----------------------------- */
        case T1_PICK:
            g->sunSpawnTimer = TARGET_FPS * 30;             /* no sky sun yet           */
            wave->nextSpawnTimer = TARGET_FPS * 5;          /* no zombie yet            */
            wave->spawnIntervalTotal = TARGET_FPS * 5;
            if (CountPlants(g, true, PLANT_NONE) > 0) Go(T1_NICE);
            break;

        case T1_NICE:
            g->sunSpawnTimer = TARGET_FPS * 30;
            wave->nextSpawnTimer = TARGET_FPS * 5;
            wave->spawnIntervalTotal = TARGET_FPS * 5;
            if (s_timer >= kNiceFrames)
            {
                g->sunSpawnTimer = 1;                       /* first sun falls now      */
                Go(T1_SUN);
            }
            break;

        case T1_SUN:
        case T1_KEEP:
        case T1_ENOUGH:
            wave->nextSpawnTimer = TARGET_FPS * 5;          /* still no zombie          */
            wave->spawnIntervalTotal = TARGET_FPS * 5;
            if (g->sunSpawnTimer > kTutorialSunGap) g->sunSpawnTimer = kTutorialSunGap;
            if (s_step == T1_SUN && sun > s_prevSun)
                Go(T1_KEEP);
            else if (s_step == T1_KEEP && sun >= COST_PEASHOOTER)
                Go(T1_ENOUGH);
            else if (s_step == T1_ENOUGH && holding && !s_prevHold)   /* clicked a seed packet */
            {
                Go(T1_ZOMBIE_WAIT);
            }
            break;

        case T1_ZOMBIE_WAIT:
            wave->nextSpawnTimer = TARGET_FPS * 5;
            wave->spawnIntervalTotal = TARGET_FPS * 5;
            if (s_timer >= kZombieDelay)
            {
                wave->nextSpawnTimer = 1;                   /* first zombie arrives     */
                wave->spawnIntervalTotal = 1;
                Go(T1_ZOMBIE_WARN);
            }
            break;

        case T1_ZOMBIE_WARN:
            if (s_timer >= kZombieWarnFrames) Go(T_OFF);
            break;

        /* ----------------------------- 1-2 ----------------------------- */
        case T2_SUNFLOWER:
            if (CountPlants(g, false, PLANT_SUNFLOWER) >= 1) Go(T2_WAIT_FIRST);
            break;

        case T2_WAIT_FIRST:
            if (CountPlants(g, false, PLANT_SUNFLOWER) >= 2) Go(T2_MORE);
            else if (s_timer >= kTry3Delay) Go(T2_TRY3);
            break;

        case T2_TRY3:
            if (CountPlants(g, false, PLANT_SUNFLOWER) >= 2 ) Go(T2_MORE);
            else if (s_timer >= kTry3MaxFrames) Go(T_OFF);
            break;

        case T2_MORE:
            if (s_timer >= kMoreFrames) Go(T_OFF);
            break;

        default: break;
    }

    /* lane darkness (1-1 only): dark while waiting for the first plant, normal
     * colour while a seed is in hand, and gone for good once a plant is down */
    {
        f32 target = (s_step == T1_PICK && !holding) ? 1.0f : 0.0f;
        s_dim += (target - s_dim) * 0.18f;
        if (fabsf(target - s_dim) < 0.01f) s_dim = target;
    }

    s_prevSun  = sun;
    s_prevHold = holding;
}

/** Darkens the playable lane(s) of 1-1 until a seed is picked up. */
extern "C" void Tutorial_DrawLaneDim(const GameContext* g)
{
    if (!g || s_dim <= 0.01f) return;
    if (g->currentLevelIndex != 0 || !IsAdventure(g)) return;
    const u32 a = (u32)(0x78 * s_dim);
    GRRLIB_Rectangle((f32)g->lawnX, (f32)g->lawnY,
                     (f32)(g->cellW * GRID_COLS), (f32)(g->cellH * g->rowCount),
                     0x00000000u | a, true);
}

/** Arrow + message. Call after the HUD so it sits on top of the seed bar. */
extern "C" void Tutorial_Draw(const GameContext* g)
{
    if (!g || s_step == T_OFF || g->state != GAME_STATE_PLAYING) return;

    /* arrow: on the packet in 1-1 until it is picked up; on the sunflower packet in 1-2 */
    int slot = -1;
    if (s_step == T1_PICK && !g->isHoldingSeed) slot = 0;
    else if (s_step == T2_SUNFLOWER && !g->isHoldingSeed) slot = SunflowerSlot(g);
    if (slot >= 0 && slot < (int)g->selectedCount)
    {
        const f32 slotX = (f32)(SEED_BAR_PADDING + 130 + slot * (SEED_SLOT_WIDTH + SEED_BAR_PADDING));
        const f32 bob = sinf((f32)g->frameCount * 0.15f) * 4.0f + 4.0f;
        DrawArrowUp(slotX + SEED_SLOT_WIDTH * 0.5f, (f32)SEED_BAR_HEIGHT + 2.0f + bob);
    }

    const char* msg = Message();
    if (msg) DrawMessage(g, msg);
}
