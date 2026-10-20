/**
 * screens.cpp - Title / game-over / mini-game art and the two special
 * level backgrounds.
 *
 * SELF-CONTAINED: no header or struct changes; state is file-local.
 *
 * Textures (assets/ ...), see tools/make_screen_assets.py for the derived ones:
 *   ui/title_art.png        640x480 key art        (derived)
 *   ui/title_logo.png       keyed logo, 420x68     (derived)
 *   ui/mg_<name>.png        80x68 mini-game icons  (derived)
 *   other/game over screen.png                     (used as-is)
 *   background/roof (final fight).png              (used as-is, 5-10)
 *   background/no lawn day.png                     (used as-is, 1-1 sod intro)
 * Every function returns false when its art is missing so the caller keeps
 * its old placeholder.
 *
 * Hooks:
 *   render.cpp  Render_Init: Screens_Load();  Render_Shutdown: Screens_Free();
 *               DrawWorldBackground: if (Screens_DrawBackground(game, bg)) return;
 *               game-over overlay:   if (!Screens_DrawGameOver(game)) { old box }
 *   menu.cpp    Menu_Render title branch: Screens_DrawTitleBg / Screens_DrawTitleLogo
 *               Minigame select:          Screens_DrawMinigameIcon
 */
#include "game.h"
#include "constants.h"
#include "level.h"
#include "backdrop.h"
#include <grrlib.h>
#include <stdio.h>
#include <string.h>
#include <math.h>

extern "C" GRRLIB_texImg* Render_LoadTexture(const char* relPath);

#ifndef TARGET_FPS
#define TARGET_FPS 60
#endif

enum { MG_COUNT = 5 };
static const char* kMgNames[MG_COUNT] = {
    "big_trouble", "nimble_quick", "column_like", "invisi_ghoul", "last_stand"
};

static GRRLIB_texImg *s_titleArt, *s_titleLogo, *s_gameOver, *s_finalFight, *s_noLawn;
static GRRLIB_texImg *s_mg[MG_COUNT];

extern "C" void Screens_Load(void)
{
    s_titleArt   = Render_LoadTexture("assets/ui/title_art.png");
    s_titleLogo  = Render_LoadTexture("assets/ui/title_logo.png");
    s_gameOver   = Render_LoadTexture("assets/other/game over screen.png");
    s_finalFight = Render_LoadTexture("assets/background/roof (final fight).png");
    s_noLawn     = Render_LoadTexture("assets/background/no lawn day.png");
    for (int i = 0; i < MG_COUNT; ++i)
    {
        char p[80];
        snprintf(p, sizeof(p), "assets/ui/mg_%s.png", kMgNames[i]);
        s_mg[i] = Render_LoadTexture(p);
    }
}

extern "C" void Screens_Free(void)
{
    GRRLIB_texImg** all[] = { &s_titleArt, &s_titleLogo, &s_gameOver, &s_finalFight, &s_noLawn };
    for (unsigned i = 0; i < sizeof(all) / sizeof(all[0]); ++i)
        if (*all[i]) { GRRLIB_FreeTexture(*all[i]); *all[i] = NULL; }
    for (int i = 0; i < MG_COUNT; ++i)
        if (s_mg[i]) { GRRLIB_FreeTexture(s_mg[i]); s_mg[i] = NULL; }
}

static bool Ok(const GRRLIB_texImg* t) { return t && t->data; }

/* Axis-aligned quad: top-left (x,y), size w*h. */
static void Draw(GRRLIB_texImg* t, f32 x, f32 y, f32 w, f32 h, u32 color)
{
    guVector q[4] = { { x, y, 0 }, { x + w, y, 0 }, { x + w, y + h, 0 }, { x, y + h, 0 } };
    GRRLIB_DrawImgQuad(q, t, color);
}

/* ------------------------------------------------------------------ */
/* Title screen / mini-game menu                                       */
/* ------------------------------------------------------------------ */
extern "C" bool Screens_DrawTitleBg(void)
{
    if (!Ok(s_titleArt)) return false;
    Draw(s_titleArt, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0xFFFFFFFF);
    return true;
}

extern "C" bool Screens_DrawTitleLogo(void)
{
    if (!Ok(s_titleLogo)) return false;
    Draw(s_titleLogo, (SCREEN_WIDTH - s_titleLogo->w) * 0.5f, 8,
         (f32)s_titleLogo->w, (f32)s_titleLogo->h, 0xFFFFFFFF);
    return true;
}

/** Icon for MinigameType `mg` (0..4) with its top-left at (x,y), drawn at `scale`. */
extern "C" bool Screens_DrawMinigameIcon(int mg, int x, int y, f32 scale)
{
    if (mg < 0 || mg >= MG_COUNT || !Ok(s_mg[mg])) return false;
    Draw(s_mg[mg], (f32)x, (f32)y, s_mg[mg]->w * scale, s_mg[mg]->h * scale, 0xFFFFFFFF);
    return true;
}

/* ------------------------------------------------------------------ */
/* Game over                                                            */
/* ------------------------------------------------------------------ */
extern "C" bool Screens_DrawGameOver(const GameContext* game)
{
    if (!game || !Ok(s_gameOver)) return false;
    /* levelIntermissionTimer counts down from TARGET_FPS*2 after the loss. */
    const f32 total = (f32)(TARGET_FPS * 2);
    f32 p = 1.0f - (f32)game->levelIntermissionTimer / total;
    if (p < 0.0f) p = 0.0f;
    if (p > 1.0f) p = 1.0f;

    GRRLIB_Rectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0x00000000 | (u32)(0xB0 * p), true);

    /* pop-in with a little overshoot (ease-out-back) over the first third */
    f32 t = p * 3.0f; if (t > 1.0f) t = 1.0f;
    const f32 c1 = 1.70158f, c3 = c1 + 1.0f;
    f32 e = 1.0f + c3 * powf(t - 1.0f, 3.0f) + c1 * powf(t - 1.0f, 2.0f);
    f32 sc = 0.78f * e;
    f32 w = s_gameOver->w * sc, h = s_gameOver->h * sc;
    u32 a = (u32)(255.0f * (t > 1.0f ? 1.0f : t));
    Draw(s_gameOver, (SCREEN_WIDTH - w) * 0.5f, (SCREEN_HEIGHT - h) * 0.5f, w, h, 0xFFFFFF00 | a);
    return true;
}

/* ------------------------------------------------------------------ */
/* Backgrounds                                                          */
/* ------------------------------------------------------------------ */
static GameState s_lastState = GAME_STATE_SELECTING;
static u32       s_wipeStart = 0;
#define SOD_WIPE_FRAMES 150

/** worldBg = the world's normal background texture (owned by render.cpp).
 *  Returns true if it drew the whole background itself.
 *
 *  Nothing here stretches the art: every picture goes through the lawn camera
 *  (backdrop.cpp), so it keeps its proportions and the lawn lanes keep their
 *  height. */
extern "C" bool Screens_DrawBackground(const GameContext* game, GRRLIB_texImg* worldBg)
{
    if (!game) return false;

    static int s_lastLvl = -1;
    if ((game->state == GAME_STATE_PLAYING && s_lastState != GAME_STATE_PLAYING) ||
        game->currentLevelIndex != s_lastLvl || game->frameCount < s_wipeStart)
        s_wipeStart = game->frameCount;
    s_lastState = game->state;
    s_lastLvl = game->currentLevelIndex;

    const int lvl = game->currentLevelIndex;

    /* 5-10 (Dr. Zomboss): final-fight roof. Adventure levels are 0..49. */
    if (lvl == TOTAL_LEVELS - 1 && Ok(s_finalFight))
    {
        const BackdropLayout L = Backdrop_Make(BD_ROOF, s_finalFight);
        Backdrop_Draw(s_finalFight, &L, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);
        return true;
    }

    /* Early Day levels play on only 1 or 3 lanes (1-1 .. 1-3): the yard is the
     * bare-dirt picture, and the sod is laid only on the lanes in play -- for
     * 1-1 it "rolls out" left to right over the first ~2.5 s. */
    if (Level_GetWorld(lvl) == WORLD_DAY && game->rowCount > 0 && game->rowCount < 5 &&
        Ok(s_noLawn) && Ok(worldBg))
    {
        const BackdropLayout L = Backdrop_Make(BD_DAY, worldBg);
        Backdrop_Draw(s_noLawn, &L, 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT);

        f32 t = 1.0f;
        if (lvl == 0 && game->state == GAME_STATE_PLAYING && game->frameCount >= s_wipeStart)
        {
            t = (f32)(game->frameCount - s_wipeStart) / (f32)SOD_WIPE_FRAMES;
            if (t > 1.0f) t = 1.0f;
        }
        const int gx = game->lawnX, gy = game->lawnY;
        const int gw = game->cellW * GRID_COLS, gh = game->cellH * game->rowCount;
        const int w  = (int)(gw * t);
        if (w > 0)
            Backdrop_Draw(worldBg, &L, gx, gy, w, gh);
        if (t < 1.0f)
        {
            /* the rolled-up sod at the leading edge */
            f32 ex = (f32)(gx + w);
            GRRLIB_Rectangle(ex - 6, (f32)gy, 12, (f32)gh, 0x2E5A16FF, true);
            GRRLIB_Rectangle(ex - 3, (f32)gy, 4, (f32)gh, 0x5FA02FFF, true);
        }
        return true;
    }
    return false;
}
