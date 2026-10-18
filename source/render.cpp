/**
 * render.cpp - GRRLIB 2D drawing for lawn, seed bar, plants, falling suns,
 * and HUD.
 *
 * Every sprite is loaded through DrawTexturedBox(), which tries a PNG from
 * assets/ first and falls back to a flat-colored rectangle if the file
 * doesn't exist yet -- see ASSETS.md at the project root for the exact
 * paths, filenames, and recommended dimensions for every image this game
 * uses. Nothing here needs to change when the art is dropped in; textures
 * are picked up automatically the next time the app starts.
 */
#include "render.h"
#include "constants.h"
#include "zombie.h"
#include "level.h"
#include "minigame.h"
#include "particle.h"
#include "survival.h"
#include "bitmap_font.h"
#include "discfst.h"
#include "progress_art.h"

/* loadscreen.cpp: small loading screen shown during Render_Init() */
void Loading_Set(float progress);
void Loading_Finish(void);
/* projfx.cpp: real projectile sprites + impact effects */
extern "C" {
void ProjFx_Load(void);
void ProjFx_Free(void);
void ProjFx_BeginFrame(const GameContext* game);
bool ProjFx_DrawProjectile(const GameContext* game, int i, f32 sx, f32 sy);
void ProjFx_DrawEffects(const GameContext* game);
}
#include <grrlib.h>
#include <stdio.h>
#include <cstdio>
#include <math.h>
#include <string.h>
#include <math.h>
#include <stdlib.h>
/* -------------------------------------------------------------------------- */
/* Placeholder color palette (used whenever the matching PNG is absent)      */
/* -------------------------------------------------------------------------- */
static const u32 COL_BG        = 0x5EA63AFF;
static const u32 COL_LAWN_DARK = 0x4A8C2AFF;
static const u32 COL_GRID_LINE = 0x00000055;
static const u32 COL_SEED_BAR  = 0x5C4033FF;
static const u32 COL_SLOT_HOVER= 0xCCAA33FF;
static const u32 COL_SLOT_SEL  = 0xFFFFFFFF;
static const u32 COL_SUN_TEXT  = 0xFFCC00FF;
static const u32 COL_HUD_TEXT  = 0xFFFFFFFF;
static const u32 COL_CURSOR    = 0xFFFFFFFF;
static const u32 COL_VALID     = 0x33DD33FF;
static const u32 COL_INVALID   = 0xDD3333FF;
static u32 PlantColor(PlantType type)
{
    switch (type)
    {
        case PLANT_PEASHOOTER:  return 0x2ECC40FF;
        case PLANT_SUNFLOWER:   return 0xFFD700FF;
        case PLANT_WALLNUT:     return 0xC49A6CFF;
        case PLANT_CHERRY_BOMB: return 0xFF4136FF;
        case PLANT_REPEATER:    return 0x2E8B57FF;
        case PLANT_SNOW_PEA:    return 0x7FDBFFFF;
        default:                return 0xFFFFFFFF;
    }
}
static PlantType SlotToPlantDisplay(const GameContext* game, s8 slot)
{
    if (slot < 0 || slot >= (s8)game->selectedCount)
        return PLANT_NONE;
    return game->selectedPlants[slot];
}
/* -------------------------------------------------------------------------- */
/* Optional runtime-loaded assets (see ASSETS.md for exact paths/sizes)      */
/* -------------------------------------------------------------------------- */
typedef struct {
    GRRLIB_texImg* seedIcon[PLANT_TYPE_COUNT];
    GRRLIB_texImg* plantSprite[PLANT_TYPE_COUNT];
    GRRLIB_texImg* sunIcon;
    GRRLIB_texImg* cursorIcon;
    /* Adventure Mode */
    GRRLIB_texImg* worldBg[WORLD_COUNT];
    GRRLIB_texImg* zombieFull[ZOMBIE_TYPE_COUNT]; /* fallback: single-texture cutout */
    /* Per-type body-part rig for procedural animation.
     * Each zombie type now has its own parts/ subdirectory. */
    GRRLIB_texImg* rigHead[ZOMBIE_TYPE_COUNT];
    GRRLIB_texImg* rigTorso[ZOMBIE_TYPE_COUNT];
    GRRLIB_texImg* rigArm[ZOMBIE_TYPE_COUNT];
    GRRLIB_texImg* rigLegA[ZOMBIE_TYPE_COUNT];
    GRRLIB_texImg* rigLegB[ZOMBIE_TYPE_COUNT];
    /* Shared pea-ball sprite used by every "pea-shaped" projectile kind
     * (PROJ_PEA/SNOW/FIRE/FUME -- see DrawProjectiles). Tinted per-kind via
     * ProjectileColor() at draw time instead of needing 4 separate files. */
    GRRLIB_texImg* projPea;
    /* Level-progress meter (assets/ui/progress_*.png) */
    GRRLIB_texImg* pbFrame;
    GRRLIB_texImg* pbFill;
    GRRLIB_texImg* pbZombie;
    GRRLIB_texImg* pbFlag;
    GRRLIB_texImg* pbLabel;
} RenderAssets;
static RenderAssets     s_assets;
static GRRLIB_ttfFont*  s_font = NULL;
static GRRLIB_texImg*   s_bitmapFont = NULL;
static unsigned         s_missingTextureCount = 0;
/* [DEBUG] Remembers the relative path of the last few Render_LoadTexture()
 * failures so the on-screen HUD can name them (see Render_GetMissingTexturePath
 * / DrawHud) instead of only showing a count. Delete alongside the counter
 * once art loading is confirmed working on real hardware/SD card. */
#define MISSING_TEXTURE_LOG_CAP 4
static char             s_missingTexturePaths[MISSING_TEXTURE_LOG_CAP][56];
static unsigned         s_missingTexturePathCount = 0;
/* Raw bytes of whichever font.ttf was loaded, kept alive for as long as
 * s_font is. See the LoadTTFKeepingBuffer() comment below for why this
 * has to exist at all. */
static u8*               s_fontFileData = NULL;
unsigned Render_GetMissingTextureCount(void)
{
    return s_missingTextureCount;
}
const char* Render_GetMissingTexturePath(unsigned index)
{
    if (index >= s_missingTexturePathCount || index >= MISSING_TEXTURE_LOG_CAP)
        return NULL;
    return s_missingTexturePaths[index];
}
/* Reads just the PNG header (IHDR) of `path` and reports whether the image
 * size is a multiple of 4 in both directions.
 *
 * WHY THIS EXISTS: GRRLIB_LoadTexturePNG() decodes through
 * PNGU_DecodeTo4x4RGBA8(), which rounds the size up to a multiple of 4 for
 * the Wii GPU. When it has to round, GRRLIB does
 *     memset(tex->data, 0, ...)   // "the texture will be completely transparent"
 * and still returns a perfectly valid, non-NULL texture. So a 150x160 seed
 * icon "loads fine" (the missing-texture counter stays at 0) and then draws
 * nothing at all -- exactly the "debug squares show only their pink backing
 * rectangle" symptom. The art in assets/ is padded to multiples of 4 (see
 * tools/pad_textures.py); this check makes sure a future un-padded PNG gets
 * reported on screen instead of failing silently again.
 * Returns true if OK or if the header couldn't be read (not a PNG, etc.). */
static bool PngSizeIsMultipleOf4(const char* path)
{
    FILE* f = fopen(path, "rb");
    if (!f) return true;
    unsigned char hdr[24];
    size_t n = fread(hdr, 1, sizeof(hdr), f);
    fclose(f);
    static const unsigned char kSig[8] = { 0x89, 'P', 'N', 'G', 0x0D, 0x0A, 0x1A, 0x0A };
    if (n < sizeof(hdr) || memcmp(hdr, kSig, 8) != 0) return true;
    u32 w = ((u32)hdr[16] << 24) | ((u32)hdr[17] << 16) | ((u32)hdr[18] << 8) | (u32)hdr[19];
    u32 h = ((u32)hdr[20] << 24) | ((u32)hdr[21] << 16) | ((u32)hdr[22] << 8) | (u32)hdr[23];
    return ((w & 3u) == 0) && ((h & 3u) == 0);
}

static void LogBadTexture(const char* tag, const char* relPath)
{
    s_missingTextureCount++;
    if (s_missingTexturePathCount < MISSING_TEXTURE_LOG_CAP)
    {
        snprintf(s_missingTexturePaths[s_missingTexturePathCount],
                 sizeof(s_missingTexturePaths[s_missingTexturePathCount]), "%s%s", tag, relPath);
        s_missingTexturePathCount++;
    }
}

GRRLIB_texImg* Render_LoadTexture(const char* relPath)
{
    if (!relPath) return NULL;
    GRRLIB_texImg* tex = GRRLIB_LoadTextureFromFile(relPath);
    if (tex && tex->data)
    {
        if (!PngSizeIsMultipleOf4(relPath))
            LogBadTexture("[x4] ", relPath); /* loaded, but will draw invisible */
        return tex;
    }
    if (tex) GRRLIB_FreeTexture(tex);

    /* Same locations main.cpp's FixWorkingDirectory() already knows about
     * -- if the chdir there didn't stick for some reason, an absolute path
     * into one of these may still resolve. "dvd:/" and "wiidisc:/" only
     * resolve if FixWorkingDirectory() already got that far (ISO9660_Mount()
     * or DiscFST_Mount() respectively); if it didn't, these are just
     * harmless failed lookups. */
    static const char* kFallbackRoots[] = { "sd:/apps/pvz_wii/", "usb:/apps/pvz_wii/", "dvd:/", "wiidisc:/" };
    char path[256];
    for (size_t i = 0; i < sizeof(kFallbackRoots) / sizeof(kFallbackRoots[0]); ++i)
    {
        snprintf(path, sizeof(path), "%s%s", kFallbackRoots[i], relPath);
        tex = GRRLIB_LoadTextureFromFile(path);
        if (tex && tex->data)
        {
            if (!PngSizeIsMultipleOf4(path))
                LogBadTexture("[x4] ", relPath);
            return tex;
        }
        if (tex) GRRLIB_FreeTexture(tex);
    }
    LogBadTexture("", relPath);
    return NULL;
}
/* NOTE: this array MUST have exactly PLANT_TYPE_COUNT entries, one per
 * PlantType in enum order (see types.h). Previously only the first 8 were
 * given explicit initializers; in C/C++ that silently zero-fills (NULL)
 * every entry after the last one supplied, so all 41 remaining plants
 * (Puff-shroom, Sun-shroom, Fume-shroom, Grave Buster, Hypno-shroom,
 * Scaredy-shroom, Ice-shroom, Gatling Pea, Jalapeno, etc.) had no seed icon
 * at all and nobody noticed at compile time. Every slot below is now listed
 * explicitly with a real path, and all PLANT_TYPE_COUNT plants now have
 * matching art in assets/seeds/ -- see ASSETS.md for where each file came
 * from (five of them -- Doom-shroom, Plantern, Cactus, Gloom-shroom, Cob
 * Cannon -- are original placeholder icons, since no reference art for
 * those five existed in either source archive). If a slot ever needs to
 * fall back to NULL again (art removed, etc.), that's still safe -- the
 * renderer draws a flat placeholder box instead of failing to load. */
static const char* s_seedIconPaths[PLANT_TYPE_COUNT] = {
    "assets/seeds/peashooter.png",     /* PLANT_PEASHOOTER */
    "assets/seeds/sunflower.png",      /* PLANT_SUNFLOWER */
    "assets/seeds/cherrybomb.png",     /* PLANT_CHERRY_BOMB */
    "assets/seeds/wallnut.png",        /* PLANT_WALLNUT */
    "assets/seeds/potato_mine.png",    /* PLANT_POTATO_MINE */
    "assets/seeds/snowpea.png",        /* PLANT_SNOW_PEA */
    "assets/seeds/chomper.png",        /* PLANT_CHOMPER */
    "assets/seeds/repeater.png",       /* PLANT_REPEATER */
    "assets/seeds/puff_shroom.png",    /* PLANT_PUFF_SHROOM */
    "assets/seeds/sun_shroom.png",     /* PLANT_SUN_SHROOM */
    "assets/seeds/fume_shroom.png",    /* PLANT_FUME_SHROOM */
    "assets/seeds/grave_buster.png",   /* PLANT_GRAVE_BUSTER */
    "assets/seeds/hypno_shroom.png",   /* PLANT_HYPNO_SHROOM */
    "assets/seeds/scaredy_shroom.png", /* PLANT_SCAREDY_SHROOM */
    "assets/seeds/ice_shroom.png",     /* PLANT_ICE_SHROOM */
    "assets/seeds/doom_shroom.png",    /* PLANT_DOOM_SHROOM */
    "assets/seeds/lily_pad.png",       /* PLANT_LILY_PAD */
    "assets/seeds/squash.png",         /* PLANT_SQUASH */
    "assets/seeds/threepeater.png",    /* PLANT_THREEPEATER */
    "assets/seeds/tangle_kelp.png",    /* PLANT_TANGLE_KELP */
    "assets/seeds/jalapeno.png",       /* PLANT_JALAPENO */
    "assets/seeds/spikeweed.png",      /* PLANT_SPIKEWEED */
    "assets/seeds/torchwood.png",      /* PLANT_TORCHWOOD */
    "assets/seeds/tallnut.png",        /* PLANT_TALLNUT */
    "assets/seeds/sea_shroom.png",     /* PLANT_SEA_SHROOM */
    "assets/seeds/plantern.png",       /* PLANT_PLANTERN */
    "assets/seeds/cactus.png",         /* PLANT_CACTUS */
    "assets/seeds/blover.png",         /* PLANT_BLOVER */
    "assets/seeds/split_pea.png",      /* PLANT_SPLIT_PEA */
    "assets/seeds/starfruit.png",      /* PLANT_STARFRUIT */
    "assets/seeds/pumpkin.png",        /* PLANT_PUMPKIN */
    "assets/seeds/magnet_shroom.png",  /* PLANT_MAGNET_SHROOM */
    "assets/seeds/cabbage_pult.png",   /* PLANT_CABBAGE_PULT */
    "assets/seeds/flower_pot.png",     /* PLANT_FLOWER_POT */
    "assets/seeds/kernel_pult.png",    /* PLANT_KERNEL_PULT */
    "assets/seeds/coffee_bean.png",    /* PLANT_COFFEE_BEAN */
    "assets/seeds/garlic.png",         /* PLANT_GARLIC */
    "assets/seeds/umbrella_leaf.png",  /* PLANT_UMBRELLA_LEAF */
    "assets/seeds/marigold.png",       /* PLANT_MARIGOLD */
    "assets/seeds/melon_pult.png",     /* PLANT_MELON_PULT */
    "assets/seeds/gatling_pea.png",    /* PLANT_GATLING_PEA */
    "assets/seeds/twin_sunflower.png", /* PLANT_TWIN_SUNFLOWER */
    "assets/seeds/gloom_shroom.png",   /* PLANT_GLOOM_SHROOM */
    "assets/seeds/cattail.png",        /* PLANT_CATTAIL */
    "assets/seeds/winter_melon.png",   /* PLANT_WINTER_MELON */
    "assets/seeds/gold_magnet.png",    /* PLANT_GOLD_MAGNET */
    "assets/seeds/spikerock.png",      /* PLANT_SPIKEROCK */
    "assets/seeds/cob_cannon.png",     /* PLANT_COB_CANNON */
    "assets/seeds/imitater.png",       /* PLANT_IMITATER */
};
static const char* s_plantSpritePaths[PLANT_TYPE_COUNT] = {
    "assets/plants/peashooter.png",     /* PLANT_PEASHOOTER */
    "assets/plants/sunflower.png",      /* PLANT_SUNFLOWER */
    "assets/plants/cherrybomb.png",     /* PLANT_CHERRY_BOMB */
    "assets/plants/wallnut.png",        /* PLANT_WALLNUT */
    "assets/plants/potato_mine.png",    /* PLANT_POTATO_MINE */
    "assets/plants/snowpea.png",        /* PLANT_SNOW_PEA */
    "assets/plants/chomper.png",        /* PLANT_CHOMPER */
    "assets/plants/repeater.png",       /* PLANT_REPEATER */
    "assets/plants/puff_shroom.png",    /* PLANT_PUFF_SHROOM */
    "assets/plants/sun_shroom.png",     /* PLANT_SUN_SHROOM */
    "assets/plants/fume_shroom.png",    /* PLANT_FUME_SHROOM */
    "assets/plants/grave_buster.png",   /* PLANT_GRAVE_BUSTER */
    "assets/plants/hypno_shroom.png",   /* PLANT_HYPNO_SHROOM */
    "assets/plants/scaredy_shroom.png", /* PLANT_SCAREDY_SHROOM */
    "assets/plants/ice_shroom.png",     /* PLANT_ICE_SHROOM */
    "assets/plants/doom_shroom.png",    /* PLANT_DOOM_SHROOM */
    "assets/plants/lily_pad.png",       /* PLANT_LILY_PAD */
    "assets/plants/squash.png",         /* PLANT_SQUASH */
    "assets/plants/threepeater.png",    /* PLANT_THREEPEATER */
    "assets/plants/tangle_kelp.png",    /* PLANT_TANGLE_KELP */
    "assets/plants/jalapeno.png",       /* PLANT_JALAPENO */
    "assets/plants/spikeweed.png",      /* PLANT_SPIKEWEED */
    "assets/plants/torchwood.png",      /* PLANT_TORCHWOOD */
    "assets/plants/tallnut.png",        /* PLANT_TALLNUT */
    "assets/plants/sea_shroom.png",     /* PLANT_SEA_SHROOM */
    "assets/plants/plantern.png",       /* PLANT_PLANTERN */
    "assets/plants/cactus.png",         /* PLANT_CACTUS */
    "assets/plants/blover.png",         /* PLANT_BLOVER */
    "assets/plants/split_pea.png",      /* PLANT_SPLIT_PEA */
    "assets/plants/starfruit.png",      /* PLANT_STARFRUIT */
    "assets/plants/pumpkin.png",        /* PLANT_PUMPKIN */
    "assets/plants/magnet_shroom.png",  /* PLANT_MAGNET_SHROOM */
    "assets/plants/cabbage_pult.png",   /* PLANT_CABBAGE_PULT */
    "assets/plants/flower_pot.png",     /* PLANT_FLOWER_POT */
    "assets/plants/kernel_pult.png",    /* PLANT_KERNEL_PULT */
    "assets/plants/coffee_bean.png",    /* PLANT_COFFEE_BEAN */
    "assets/plants/garlic.png",         /* PLANT_GARLIC */
    "assets/plants/umbrella_leaf.png",  /* PLANT_UMBRELLA_LEAF */
    "assets/plants/marigold.png",       /* PLANT_MARIGOLD */
    "assets/plants/melon_pult.png",     /* PLANT_MELON_PULT */
    "assets/plants/gatling_pea.png",    /* PLANT_GATLING_PEA */
    "assets/plants/twin_sunflower.png", /* PLANT_TWIN_SUNFLOWER */
    "assets/plants/gloom_shroom.png",   /* PLANT_GLOOM_SHROOM */
    "assets/plants/cattail.png",        /* PLANT_CATTAIL */
    "assets/plants/winter_melon.png",   /* PLANT_WINTER_MELON */
    "assets/plants/gold_magnet.png",    /* PLANT_GOLD_MAGNET */
    "assets/plants/spikerock.png",      /* PLANT_SPIKEROCK */
    "assets/plants/cob_cannon.png",     /* PLANT_COB_CANNON */
    "assets/plants/imitater.png",       /* PLANT_IMITATER */
};
static const char* s_worldBgPaths[WORLD_COUNT] = {
    "assets/background/every lawn day.png",
    "assets/background/night.png",
    "assets/background/pool.png",
    "assets/background/fog.png",
    "assets/background/roof.png",
};
/* -------------------------------------------------------------------------- */
/* Small drawing helpers                                                     */
/* -------------------------------------------------------------------------- */
static void DrawRect(s16 x, s16 y, s16 w, s16 h, u32 color)
{
    GRRLIB_Rectangle((f32)x, (f32)y, (f32)w, (f32)h, color, true);
}
static void DrawRectOutline(s16 x, s16 y, s16 w, s16 h, u32 color, u8 thickness)
{
    for (u8 t = 0; t < thickness; ++t)
        GRRLIB_Rectangle((f32)(x + t), (f32)(y + t), (f32)(w - t * 2), (f32)(h - t * 2), color, false);
}
static void DrawDashedBreathingBorder(s16 x, s16 y, s16 w, s16 h, u32 color, u8 dashLen, u32 frameCount)
{
    if (w <= 0 || h <= 0) return;
    f32 breath = sinf((f32)frameCount * 0.04f) * 2.5f;
    s16 curDash = (s16)((f32)dashLen + breath);
    if (curDash < 2) curDash = 2;
    s16 px = x;
    for (s16 dx = 0; dx < w; )
    {
        s16 seg = (dx + curDash <= w) ? curDash : (w - dx);
        GRRLIB_Line((f32)px, (f32)y, (f32)(px + seg - 1), (f32)y, color);
        px += seg; dx += seg;
        s16 gap = (dx + curDash <= w) ? curDash : (w - dx);
        px += gap; dx += gap;
    }
    px = x;
    for (s16 dx = 0; dx < w; )
    {
        s16 seg = (dx + curDash <= w) ? curDash : (w - dx);
        GRRLIB_Line((f32)px, (f32)(y + h - 1), (f32)(px + seg - 1), (f32)(y + h - 1), color);
        px += seg; dx += seg;
        s16 gap = (dx + curDash <= w) ? curDash : (w - dx);
        px += gap; dx += gap;
    }
    s16 py = y;
    for (s16 dy = 0; dy < h; )
    {
        s16 seg = (dy + curDash <= h) ? curDash : (h - dy);
        GRRLIB_Line((f32)x, (f32)py, (f32)x, (f32)(py + seg - 1), color);
        py += seg; dy += seg;
        s16 gap = (dy + curDash <= h) ? curDash : (h - dy);
        py += gap; dy += gap;
    }
    py = y;
    for (s16 dy = 0; dy < h; )
    {
        s16 seg = (dy + curDash <= h) ? curDash : (h - dy);
        GRRLIB_Line((f32)(x + w - 1), (f32)py, (f32)(x + w - 1), (f32)(py + seg - 1), color);
        py += seg; dy += seg;
        s16 gap = (dy + curDash <= h) ? curDash : (h - dy);
        py += gap; dy += gap;
    }
}
static f32 BitmapFontZoomForSize(unsigned int ttfSize)
{
    return (f32)ttfSize / 8.0f;
}
static void DrawLabel(s16 x, s16 y, u32 color, unsigned int size, const char* text)
{
    if (!text) return;
    /* Real TTF (assets/font/pvz_font.ttf) first when it loaded -- the
     * built-in 8x8 bitmap font is only meant as a fallback for when no
     * font.ttf is present (see Render_Init below). s_bitmapFont is always
     * non-NULL (it doesn't depend on any file), so checking it first would
     * always win and the TTF would never actually get used. */
    if (s_font)
        GRRLIB_PrintfTTF(x, y, s_font, text, size, color);
    else if (s_bitmapFont)
        GRRLIB_Printf((f32)x, (f32)y, s_bitmapFont, color, BitmapFontZoomForSize(size), "%s", text);
}
/* Draws `tex` as an explicit screen-space quad (GRRLIB_DrawImgQuad), so the
 * result does not depend on GRRLIB's handle/offset behaviour -- GRRLIB_DrawImg()
 * scales and rotates around the image CENTRE, and combined with
 * GRRLIB_SetHandle() that put every scaled sprite (the planted Peashooter,
 * the zombies, the held seed) far down and to the right of where it was meant
 * to be. Here the point (pivotFx, pivotFy) -- as fractions of the image,
 * (0,0)=top-left, (0.5,1)=bottom-centre -- lands exactly on (px,py); the
 * sprite is scaled by (sx,sy) and then rotated by angleDeg (clockwise
 * positive) around that same point. */
static void DrawSpriteQuad(GRRLIB_texImg* tex, f32 px, f32 py, f32 pivotFx, f32 pivotFy,
                           f32 sx, f32 sy, f32 angleDeg, u32 color)
{
    if (!tex || !tex->data) return;
    const f32 w = (f32)tex->w, h = (f32)tex->h;
    const f32 lx0 = -pivotFx * w * sx,          ly0 = -pivotFy * h * sy;
    const f32 lx1 = (1.0f - pivotFx) * w * sx,  ly1 = (1.0f - pivotFy) * h * sy;
    const f32 rad = angleDeg * 3.14159265f / 180.0f;
    const f32 c = cosf(rad), sn = sinf(rad);
    const f32 cx[4] = { lx0, lx1, lx1, lx0 };
    const f32 cy[4] = { ly0, ly0, ly1, ly1 };
    guVector q[4];
    for (int i = 0; i < 4; ++i)
    {
        q[i].x = px + cx[i] * c - cy[i] * sn;
        q[i].y = py + cx[i] * sn + cy[i] * c;
        q[i].z = 0.0f;
    }
    GRRLIB_DrawImgQuad(q, tex, color);
}

/** Draws `tex` scaled to fit (w,h) at (x,y) if loaded; otherwise a flat rect.
 *  No decorative border here -- a missing texture should just look like a
 *  plain placeholder, not a permanent (and misleading) selection outline. */
static void DrawTexturedBox(GRRLIB_texImg* tex, s16 x, s16 y, s16 w, s16 h, u32 tint, u32 fallbackColor, u32 frameCount)
{
    (void)frameCount;
    if (tex && tex->data)
    {
        /* Defensive: some textures (planted plants, zombies) get a
         * bottom/top-center pivot set elsewhere via GRRLIB_SetHandle for
         * their sway/walk animation. This helper always means "draw at
         * (x,y) top-left", so it resets the pivot before every draw rather
         * than assuming nobody else touched it. */
        f32 scaleX = (tex->w > 0) ? ((f32)w / (f32)tex->w) : 1.0f;
        f32 scaleY = (tex->h > 0) ? ((f32)h / (f32)tex->h) : 1.0f;
        DrawSpriteQuad(tex, (f32)x, (f32)y, 0.0f, 0.0f, scaleX, scaleY, 0.0f, tint);
    }
    else
    {
        DrawRect(x, y, w, h, fallbackColor);
    }
}
static void DrawHealthBar(s16 cx, s16 cy, s16 barW, s16 curHP, s16 maxHP)
{
    if (maxHP <= 0) return;
    s16 barH = 5;
    s16 bgX = cx - barW / 2;
    s16 bgY = cy - barH - 2;
    GRRLIB_Rectangle(bgX, bgY, barW, barH, 0x00000080, true);
    s16 fillW = (barW * curHP) / maxHP;
    if (fillW < 0) fillW = 0;
    if (fillW > barW) fillW = barW;
    if (fillW > 0)
    {
        f32 ratio = (f32)curHP / (f32)maxHP;
        u32 fillColor;
        if (ratio > 0.6f)
            fillColor = 0x44DD44FF;
        else if (ratio > 0.3f)
            fillColor = 0xDDDD44FF;
        else
            fillColor = 0xDD4444FF;
        GRRLIB_Rectangle(bgX + 1, bgY + 1, fillW - 2, barH - 2, fillColor, true);
    }
}
/** Planted plants get a gentle idle sway/bob (pivoted at their base, set via
 *  GRRLIB_SetHandle in Render_Init) instead of sitting perfectly still --
 *  small and slow, since real per-plant animation (Peashooter's recoil,
 *  Wall-nut's cracking face, etc.) isn't implemented yet; this is just enough
 *  motion to read as "alive" rather than a pasted-on sticker. `phaseSeed`
 *  (e.g. row*7+col) staggers plants so they don't all sway in lockstep. */
/* Pea-shooter attack animation. Pea plants fire once every cooldown
 * (SHOOTER_COOLDOWN_FRAMES = 1.5 s for the Peashooter); combat.cpp sets
 * GridCell::shootTimer to the cooldown the moment a shot leaves and counts it
 * down, so "frames since the shot" = cooldown - shootTimer. We play a quick
 * recoil (lean back, squash, kick left) right after the shot, and a small
 * wind-up (lean forward, stretch) in the last few frames before the next one. */
static u16 PeaCooldownFrames(PlantType t)
{
    switch (t)
    {
        case PLANT_PEASHOOTER:
        case PLANT_SNOW_PEA:
        case PLANT_THREEPEATER:
        case PLANT_SPLIT_PEA:    return (u16)SHOOTER_COOLDOWN_FRAMES;
        case PLANT_REPEATER:     return (u16)REPEATER_COOLDOWN_FRAMES;
        case PLANT_GATLING_PEA:  return (u16)GATLING_COOLDOWN_FRAMES;
        default:                 return 0; /* not a pea plant -> no attack animation */
    }
}

typedef struct { f32 angle, sx, sy, dx, sway; } AttackPose;

static AttackPose ComputeAttackPose(PlantType type, u16 shootTimer)
{
    AttackPose p = { 0.0f, 1.0f, 1.0f, 0.0f, 1.0f };
    const u16 cooldown = PeaCooldownFrames(type);
    if (cooldown == 0 || shootTimer == 0 || shootTimer > cooldown)
        return p;

    const f32 kRecoilFrames  = 14.0f;
    const f32 kWindUpFrames  = 10.0f;
    const f32 sinceShot = (f32)(cooldown - shootTimer);

    if (sinceShot < kRecoilFrames)
    {
        f32 e = 1.0f - sinceShot / kRecoilFrames;   /* 1 right after the shot -> 0 */
        e = e * e;
        p.angle = -7.0f * e;      /* lean back (counter-clockwise)  */
        p.sx    = 1.0f - 0.12f * e; /* squash ...                   */
        p.sy    = 1.0f + 0.10f * e; /* ... and stretch              */
        p.dx    = -4.0f * e;      /* kick backwards                 */
        p.sway  = 1.0f - e;       /* calm the idle sway while firing */
    }
    else if (shootTimer <= (u16)kWindUpFrames)
    {
        f32 a = 1.0f - (f32)shootTimer / kWindUpFrames; /* 0 -> 1 as the shot nears */
        p.angle = 4.0f * a;       /* lean forward, getting ready */
        p.sy    = 1.0f + 0.04f * a;
    }
    return p;
}

static void DrawPlantInCell(GRRLIB_texImg* tex, s16 cellX, s16 cellY, s16 cellW, s16 cellH,
                            u32 frameCount, u32 phaseSeed, u32 fallbackColor,
                            PlantType type, u16 shootTimer)
{
    s16 pad = 6;
    if (tex && tex->data && tex->h > 0)
    {
        f32 footX = (f32)(cellX + cellW / 2);
        f32 footY = (f32)(cellY + cellH - pad);
        f32 phase = (f32)frameCount * 0.045f + (f32)(phaseSeed % 7) * 0.9f;
        AttackPose pose = ComputeAttackPose(type, shootTimer);
        f32 sway  = sinf(phase) * 3.0f * pose.sway;
        f32 bob   = fabsf(sinf(phase * 0.5f)) * 2.0f * pose.sway;
        f32 targetH = (f32)(cellH - pad * 2);
        f32 s = targetH / (f32)tex->h;
        DrawSpriteQuad(tex, footX + pose.dx, footY - bob, 0.5f, 1.0f,
                       s * pose.sx, s * pose.sy, sway + pose.angle, 0xFFFFFFFF);
    }
    else
    {
        DrawRect(cellX + pad, cellY + pad, cellW - pad * 2, cellH - pad * 2, fallbackColor);
    }
}
static u32 Color4ToU32(Color4 c)
{
    return ((u32)c.r << 24) | ((u32)c.g << 16) | ((u32)c.b << 8) | (u32)c.a;
}
/* -------------------------------------------------------------------------- */
/* Zombies                                                                    */
/*                                                                            */
/* SCOPE NOTE: the walk-cycle math below (bob/sway/limb-swing amplitudes,     */
/* pivot offsets) is a first pass tuned by eye against the source art's       */
/* rough proportions, not against real hardware -- expect to nudge the        */
/* numbers once you see it running on a Wii/Dolphin. See zombie.h for what    */
/* per-type behaviors are and aren't implemented yet.                        */
/* -------------------------------------------------------------------------- */
static f32 TileToScreenX(const GameContext* game, f32 tileX)
{
    return (f32)game->lawnX + tileX * (f32)game->cellW;
}
static f32 RowToScreenBottomY(const GameContext* game, u8 row)
{
    return (f32)game->lawnY + (f32)(row + 1) * (f32)game->cellH;
}
/** The ~29 non-rigged types: one static cutout, animated with a procedural
 *  bob + sway (pivot is the sprite's bottom-center, set once at load time in
 *  Render_Init via GRRLIB_SetHandle) since the source art is a single pose
 *  rather than a hand-drawn walk-cycle strip. */
static void DrawSimpleZombie(const GameContext* game, const Zombie* z)
{
    GRRLIB_texImg* tex = s_assets.zombieFull[z->type];
    f32 footX = TileToScreenX(game, z->x + 0.5f);
    f32 footY = RowToScreenBottomY(game, z->row);
    f32 phase = (f32)z->animTimer * 0.15f;
    f32 bob = 0.0f, sway = 0.0f, scale = 1.0f, alpha = 255.0f;
    if (z->state == ZSTATE_DYING)
    {
        f32 t = (f32)z->deathTimer / (f32)ZOMBIE_DEATH_FADE_FRAMES; /* 1 -> 0 */
        alpha = 255.0f * t;
        scale = 0.85f + 0.15f * t;
    }
    else if (z->state == ZSTATE_EATING)
    {
        sway = sinf(phase * 2.0f) * 4.0f; /* quicker little "gnawing" rock */
    }
    else /* walking */
    {
        bob  = fabsf(sinf(phase)) * 4.0f;
        sway = sinf(phase) * 6.0f;
    }
    u32 color = 0xFFFFFF00u | (u32)alpha;
    f32 targetH = game->cellH * 1.6f * scale;
    if (tex && tex->h > 0)
    {
        f32 s = targetH / (f32)tex->h;
        DrawSpriteQuad(tex, footX, footY - bob, 0.5f, 1.0f, s, s, sway, color);
    }
    else
    {
        f32 w = game->cellW * 0.7f;
        DrawRect((s16)(footX - w / 2), (s16)(footY - targetH), (s16)w, (s16)targetH, 0x336633FF);
    }
}
/* [DEBUG] Snapshot of the first rigged zombie drawn each frame -- which
 * parts were non-null and where they actually got told to draw -- so the
 * HUD can show whether "only legs visible" is a null-texture problem
 * (parts show N) or a positioning problem (parts show Y but the numbers
 * look wrong). Delete once the rig is confirmed fully visible. */
typedef struct {
    bool captured;
    bool hasLegA, hasLegB, hasArm, hasTorso, hasHead;
    f32 footX, hipY, shoulderY, headY, legScale;
    f32 footY, footDelta; /* footDelta = where the sole actually ends up
                            * minus footY (the row's floor line); 0 = touches
                            * exactly, positive = sole is below the floor
                            * (sinks into the tile), negative = sole is
                            * above it (floats). */
} RigDebugSnapshot;
static RigDebugSnapshot s_rigDebug;
static void Render_ResetRigDebugCapture(void) { s_rigDebug.captured = false; }
/* Draws a rig part scaled by (scaleX,scaleY) and rotated by angleDeg
 * (GRRLIB_DrawImg's own convention), positioning it so that its
 * *un-rotated* horizontal-center point at a height fraction `anchorFrac`
 * down from the top (0 = top edge, 0.75 = 3/4 down, ...) lands at
 * (targetX, targetY); rotation then happens around the part's own center.
 *
 * This exists instead of GRRLIB's built-in handle mechanism because that
 * mechanism does NOT behave like "handle point maps to xpos/ypos": its
 * actual formula shifts the result by an amount that depends on BOTH the
 * handle offset and the current scale, so a handle tuned by eye at one
 * scale silently mis-renders at another (confirmed: the rig stayed
 * gigantic/misplaced across two different handle values, and even once
 * legScale itself was confirmed sane -- pointing at the position formula,
 * not the scale computation). Using handle (0,0) sidesteps that
 * entirely: with handle (0,0), GRRLIB places the image's own center at
 * (xpos + tex->w*0.5, ypos + tex->h*0.5) regardless of scale or rotation --
 * a fixed, verifiable relationship -- so we solve for xpos/ypos ourselves
 * from that instead of trusting GRRLIB's handle to do it.
 *
 * Trade-off: a part now visually rotates around its own center rather
 * than around its top/anchor point (e.g. a leg's swing pivots from its
 * middle, not the hip). Slightly less anatomically correct, but
 * predictable and scale-independent -- worth it until confirmed working,
 * at which point this can be refined to rotate around the anchor instead. */
static void DrawRigPart(GRRLIB_texImg* tex, f32 targetX, f32 targetY, f32 anchorFrac,
                         f32 scaleX, f32 scaleY, f32 angleDeg, u32 color)
{
    if (!tex) return;
    f32 cx = targetX;
    f32 cy = targetY + (0.5f - anchorFrac) * (f32)tex->h * scaleY;
    DrawSpriteQuad(tex, cx, cy, 0.5f, 0.5f, scaleX, scaleY, angleDeg, color);
}
/** All zombie types: composited from per-type parts/ (head, torso, arm,
 *  leg_a, leg_b) and animated frame-by-frame with procedural walk-cycle
 *  limb swinging. Each zombie type now provides its own parts, so the
 *  accessory system (cone/bucket/flag/helmet) is no longer needed --
 *  each type's head.png already includes its decoration. */
__attribute__((unused)) static void DrawRiggedZombie(const GameContext* game, const Zombie* z, const ZombieDef* def)
{
    f32 footX = TileToScreenX(game, z->x + 0.5f);
    f32 footY = RowToScreenBottomY(game, z->row);
    f32 phase = (f32)z->animTimer * 0.15f;
    bool walking = (z->state == ZSTATE_WALKING);
    f32 swing = walking ? sinf(phase) : (z->state == ZSTATE_EATING ? sinf(phase * 2.0f) * 0.3f : 0.0f);
    f32 legAAngle =  swing * 22.0f;
    f32 legBAngle = -swing * 22.0f;
    f32 armAngle  = -swing * 16.0f;
    f32 bodyBob   = walking ? fabsf(swing) * 3.0f : 0.0f;
    f32 alpha = 255.0f;
    if (z->state == ZSTATE_DYING)
        alpha = 255.0f * (f32)z->deathTimer / (f32)ZOMBIE_DEATH_FADE_FRAMES;
    u32 color = 0xFFFFFF00u | (u32)alpha;
    GRRLIB_texImg* legA = s_assets.rigLegA[z->type];
    GRRLIB_texImg* legB = s_assets.rigLegB[z->type];
    GRRLIB_texImg* arm  = s_assets.rigArm[z->type];
    GRRLIB_texImg* torso = s_assets.rigTorso[z->type];
    GRRLIB_texImg* head = s_assets.rigHead[z->type];
    f32 legScale = (legA && legA->h > 0)
                       ? (game->cellH * 0.85f) / (f32)legA->h : 1.0f;
    f32 legH   = legA ? legA->h * legScale : game->cellH * 0.8f;
    /* hipY used to be "footY - legH*0.92": that 0.92 was presumably tuned
     * by eye against the old GRRLIB-handle-based positioning, which we've
     * since replaced (see DrawRigPart above) -- there's no reason it still
     * matches under the new one. Using the exact relationship instead
     * (bottom of a top-anchored, anchorFrac=0 part sits exactly legH below
     * its target) makes the sole of the leg image touch the row's floor
     * line precisely, *assuming* the art has no transparent padding below
     * the visible foot. If feet still don't touch the tile after this,
     * that assumption is the next thing to check -- see the footDelta
     * debug line below, which measures the actual gap in pixels. */
    f32 hipY   = footY - legH - bodyBob;
    DrawRigPart(legB, footX - 6, hipY, 0.0f, legScale, legScale, legBAngle, color);
    DrawRigPart(legA, footX + 6, hipY, 0.0f, legScale, legScale, legAAngle, color);
    f32 torsoH = torso ? torso->h * legScale : game->cellH * 0.7f;
    f32 shoulderY = hipY - torsoH * 0.85f;
    DrawRigPart(arm, footX - 10, shoulderY, 0.0f, -legScale, legScale, armAngle, color);
    DrawRigPart(arm, footX + 10, shoulderY, 0.0f,  legScale, legScale, -armAngle, color);
    DrawRigPart(torso, footX, shoulderY, 0.0f, legScale, legScale, 0.0f, color);
    f32 headY = shoulderY - torsoH * 0.62f;
    DrawRigPart(head, footX, headY, 0.75f, legScale, legScale, 0.0f, color);
    if (!s_rigDebug.captured)
    {
        s_rigDebug.captured  = true;
        s_rigDebug.hasLegA   = (legA != NULL);
        s_rigDebug.hasLegB   = (legB != NULL);
        s_rigDebug.hasArm    = (arm != NULL);
        s_rigDebug.hasTorso  = (torso != NULL);
        s_rigDebug.hasHead   = (head != NULL);
        s_rigDebug.footX     = footX;
        s_rigDebug.hipY      = hipY;
        s_rigDebug.shoulderY = shoulderY;
        s_rigDebug.headY     = headY;
        s_rigDebug.legScale  = legScale;
        s_rigDebug.footY     = footY;
        /* Bottom edge of the (unrotated) leg image = hipY + legH, per
         * DrawRigPart's anchorFrac=0 math. Comparing that to footY tells us
         * exactly how many pixels off the calibration above still is. */
        s_rigDebug.footDelta = (hipY + legH) - footY;
    }
}
static void DrawZombieHealth(const GameContext* game, const Zombie* z)
{
    f32 footX = TileToScreenX(game, z->x + 0.5f);
    f32 footY = RowToScreenBottomY(game, z->row);
    const ZombieDef* def = Zombie_GetDef(z->type);
    DrawHealthBar((s16)footX, (s16)(footY - game->cellH * 1.6f - 2),
                  game->cellW - 8, z->health, def->health);
}
/* The assets/zombies/<type>/parts/arm.png files are NOT arms -- they are a
 * second shoe/leg fragment cut from the wrong part of the sheet, so the rig
 * draws legs where the arms should be. Until real arm parts are cut, zombies
 * are drawn from the complete full.png body (arms included) with the same
 * bob/sway walk. Set to true to go back to the per-part rig. */
static const bool kUseRiggedZombies = false;
static void DrawZombies(const GameContext* game)
{
    Render_ResetRigDebugCapture();
    bool invisible = !Minigame_CanRenderZombie(game);
    for (int i = 0; i < MAX_ZOMBIES; ++i)
    {
        const Zombie* z = &game->zombies[i];
        if (!z->active) continue;
        if (invisible)
        {
            /* Draw a subtle shimmer for invisi-ghoul */
            s16 x = (s16)(game->lawnX + z->x * game->cellW);
            s16 y = (s16)(game->lawnY + z->row * game->cellH);
            GRRLIB_Rectangle(x + 12, y + 20, 8, 16, 0xFFFFFF10, true);
            continue;
        }
        if (kUseRiggedZombies && s_assets.rigHead[z->type] && s_assets.rigHead[z->type]->data)
            DrawRiggedZombie(game, z, Zombie_GetDef(z->type));
        else
            DrawSimpleZombie(game, z);
        DrawZombieHealth(game, z);
    }
}
/** World-themed backdrop: assets/backgrounds/<world>.png if present, else a
 *  flat tint (see Level_GetWorldTint) -- Day/Pool/Roof read as daytime,
 *  Night/Fog as night, matching the original's world progression. */
static void DrawWorldBackground(const GameContext* game)
{
    World world = Level_GetWorld(game->currentLevelIndex);
    u32 tint = Color4ToU32(Level_GetWorldTint(world));
    DrawTexturedBox(s_assets.worldBg[world], 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0xFFFFFFFF, tint, game->frameCount);
}
static void DrawLevelBanner(const GameContext* game)
{
    if (!s_bitmapFont && !s_font) return;
    char buf[64];
    if (Survival_IsSurvival(game->currentLevelIndex))
    {
        SurvivalType st = Survival_GetType(game->currentLevelIndex);
        const char* name = "";
        switch (st)
        {
            case SURVIVAL_DAY_EASY:    name = "Survival: Day (Easy)";    break;
            case SURVIVAL_DAY_HARD:    name = "Survival: Day (Hard)";    break;
            case SURVIVAL_NIGHT_EASY:  name = "Survival: Night (Easy)";  break;
            case SURVIVAL_NIGHT_HARD:  name = "Survival: Night (Hard)";  break;
            case SURVIVAL_POOL_EASY:   name = "Survival: Pool (Easy)";   break;
            case SURVIVAL_POOL_HARD:   name = "Survival: Pool (Hard)";   break;
            case SURVIVAL_FOG_EASY:    name = "Survival: Fog (Easy)";    break;
            case SURVIVAL_FOG_HARD:    name = "Survival: Fog (Hard)";    break;
            case SURVIVAL_ROOF_EASY:   name = "Survival: Roof (Easy)";   break;
            case SURVIVAL_ROOF_HARD:   name = "Survival: Roof (Hard)";   break;
            case SURVIVAL_ENDLESS:     name = "Survival: Endless";       break;
        }
        snprintf(buf, sizeof(buf), "%s  -  Flag %u", name, (unsigned)game->survivalFlags);
        DrawLabel(SCREEN_WIDTH - 170, 18, COL_HUD_TEXT, 14, buf);
    }
    else if (Minigame_IsMinigame(game->currentLevelIndex))
    {
        snprintf(buf, sizeof(buf), "Minigame");
        DrawLabel(SCREEN_WIDTH - 170, 18, COL_HUD_TEXT, 14, buf);
    }
    else
    {
        World world = Level_GetWorld(game->currentLevelIndex);
        u8 levelInWorld = (game->currentLevelIndex % LEVELS_PER_WORLD) + 1;
        snprintf(buf, sizeof(buf), "%s - Level %u", Level_WorldName(world), (unsigned)levelInWorld);
        DrawLabel(SCREEN_WIDTH - 170, 18, COL_HUD_TEXT, 14, buf);
    }
    if (game->state == GAME_STATE_LEVEL_WON)
    {
        DrawRect(SCREEN_WIDTH / 2 - 140, SCREEN_HEIGHT / 2 - 30, 280, 60, 0x000000B0);
        DrawLabel(SCREEN_WIDTH / 2 - 90, SCREEN_HEIGHT / 2 - 8, 0xFFDD33FF, 20, "Level Complete!");
    }
    else if (game->state == GAME_STATE_GAME_OVER)
    {
        DrawRect(SCREEN_WIDTH / 2 - 140, SCREEN_HEIGHT / 2 - 30, 280, 60, 0x000000B0);
        DrawLabel(SCREEN_WIDTH / 2 - 100, SCREEN_HEIGHT / 2 - 8, 0xFF4444FF, 20, "The zombies got in...");
    }
    else if (game->showHugeWaveMessage)
    {
        /* Adventure Mode: last wave incoming. Red text, no button needed --
         * it clears on its own and the wave starts a beat later. */
        DrawRect(SCREEN_WIDTH / 2 - 190, SCREEN_HEIGHT / 2 - 26, 380, 44, 0x000000B0);
        DrawLabel(SCREEN_WIDTH / 2 - 175, SCREEN_HEIGHT / 2 - 6, 0xFF2222FF, 16,
                  "A huge wave of zombies is approaching!");
    }
}
/* -------------------------------------------------------------------------- */
/* Frame sections                                                             */
/* -------------------------------------------------------------------------- */
static void DrawSeedBar(const GameContext* game, const InputState* input)
{
    DrawRect(0, 0, SCREEN_WIDTH, SEED_BAR_HEIGHT, COL_SEED_BAR);
    s16 slotY = (SEED_BAR_HEIGHT - SEED_SLOT_HEIGHT) / 2;
    /* Sun counter, left-aligned in the seed bar. Reserved as its own fixed
     * width panel -- the count can be up to 4 digits (capped at 9999), and
     * at this label size that text can run wider than the old 64px gap,
     * which was spilling into (and reading as "drawn on top of") the first
     * seed slot. Give it real breathing room before the seed tray starts. */
    s16 sunX = SEED_BAR_PADDING + 4;
    s16 sunY = (SEED_BAR_HEIGHT - 40) / 2;
    DrawTexturedBox(s_assets.sunIcon, sunX, sunY, 40, 40, 0xFFFFFFFF, COL_SUN_TEXT, game->frameCount);
    if (s_bitmapFont || s_font)
    {
        char sunBuf[8];
        snprintf(sunBuf, sizeof(sunBuf), "%u", (unsigned)game->sun);
        DrawLabel(sunX + 44, sunY + 12, COL_HUD_TEXT, 16, sunBuf);
    }
    const s16 sunPanelWidth = 130; /* clears icon + a full 4-digit count */
    u8 slots = game->selectedCount;
    if (slots > SEED_SLOT_COUNT) slots = SEED_SLOT_COUNT;
    for (s8 i = 0; i < (s8)slots; ++i)
    {
        s16 slotX = SEED_BAR_PADDING + sunPanelWidth + i * (SEED_SLOT_WIDTH + SEED_BAR_PADDING);
        PlantType type = SlotToPlantDisplay(game, i);
        if (type < 0) continue;
        bool isSelected = (i == game->selectedSlot);
        bool isHovered  = (i == input->hoveredSeedSlot);
        DrawRect(slotX, slotY, SEED_SLOT_WIDTH, SEED_SLOT_HEIGHT, 0x1A1A1AFF);
        /* Only the currently-selected packet gets the animated dashed
         * border; a hover gets a plain thin outline as a lighter cue, and
         * an idle slot gets no border decoration at all. */
        if (isSelected)
            DrawDashedBreathingBorder(slotX, slotY, SEED_SLOT_WIDTH, SEED_SLOT_HEIGHT, COL_SLOT_SEL, 10, game->frameCount);
        else if (isHovered)
            DrawRectOutline(slotX, slotY, SEED_SLOT_WIDTH, SEED_SLOT_HEIGHT, COL_SLOT_HOVER, 2);
        const s16 iconSize = 48;
        s16 iconX = slotX + (SEED_SLOT_WIDTH - iconSize) / 2;
        s16 iconY = slotY + 3;
        DrawTexturedBox(s_assets.seedIcon[type], iconX, iconY, iconSize, iconSize, 0xFFFFFFFF, PlantColor(type), game->frameCount);
        u16 cost = Game_PlantCost(type);
        bool affordable = game->sun >= cost;
        /* Was 0x00000090 (56% opaque black) -- dark enough over the icon
         * plus the already-dark 0x1A1A1A slot background that it read as
         * "no texture here" rather than "can't afford this yet" (that's
         * exactly what happened testing the plant-rendering fix: the seed
         * looked broken/missing when it was actually just fully dimmed).
         * Lighter overlay reads as "dimmed" instead of "gone". */
        if (!affordable)
            DrawRect(slotX, slotY, SEED_SLOT_WIDTH, SEED_SLOT_HEIGHT, 0x00000050);
        if (s_bitmapFont || s_font)
        {
            char costBuf[8];
            snprintf(costBuf, sizeof(costBuf), "%u", cost);
            DrawLabel(slotX + 4, slotY + SEED_SLOT_HEIGHT - 16, COL_SUN_TEXT, 12, costBuf);
        }
    }
}
/** Water columns: In Pool/Fog the middle rows (2 & 3, 0-indexed) are water. */
#define WATER_ROW_START 2
#define WATER_ROW_END   3
static void DrawWaterWaves(const GameContext* game, s8 row, s8 col, s16 x, s16 y)
{
    unsigned int fc = (unsigned int)game->frameCount;
    int w, d;
    float phase, offset, dx, dy;
    unsigned int alpha;
    for (w = 0; w < 3; w++)
    {
        phase = (float)fc * 0.03f + (float)col * 0.8f + (float)row * 1.2f + (float)w * 2.1f;
        offset = sinf(phase) * 6.0f;
        int wy = (int)(y + (int)((float)game->cellH * 0.3f) + (int)offset + w * 8);
        if (wy < y || wy > y + game->cellH) continue;
        dx = (sinf(phase * 0.5f + 1.0f) * 0.5f + 0.5f) * (float)(game->cellW - 8);
        alpha = (unsigned int)((sinf(phase * 0.7f) * 0.3f + 0.5f) * 80.0f);
        GRRLIB_Rectangle((float)(x + 2), (float)wy, (float)(game->cellW - 4), 2.0f, 0xCCEEFF00 | (alpha & 0xFF), true);
    }
    for (d = 0; d < 2; d++)
    {
        phase = (float)fc * 0.05f + (float)(col * 3 + row * 7 + d * 11);
        dx = (sinf(phase * 0.7f) * 0.5f + 0.5f) * (float)(game->cellW - 10);
        dy = (sinf(phase * 0.9f + 2.0f) * 0.5f + 0.5f) * (float)(game->cellH - 10);
        alpha = (unsigned int)((sinf(phase * 1.1f) * 0.5f + 0.5f) * 60.0f);
        GRRLIB_Rectangle((float)(x + 4 + (int)dx), (float)(y + 4 + (int)dy), 3.0f, 3.0f, 0xFFFFFF00 | (alpha & 0xFF), true);
    }
}
static void DrawCellBackground(const GameContext* game, s8 row, s8 col, s16 x, s16 y)
{
    bool isWater = (game->worldFlags & WORLD_FLAG_HAS_WATER) &&
                   (row >= WATER_ROW_START && row <= WATER_ROW_END);
    if (isWater)
    {
        DrawRect(x, y, game->cellW, game->cellH, 0x1A5276FF);
        if ((row + col) % 2 == 0)
            DrawRect(x, y, game->cellW, game->cellH, 0x2E86C120);
        DrawRect(x, y + game->cellH - 8, game->cellW, 8, 0x0E2A44B0);
        DrawWaterWaves(game, row, col, x, y);
    }
    else
    {
        if ((row + col) % 2 == 0)
            DrawRect(x, y, game->cellW, game->cellH, COL_LAWN_DARK);
    }
}
static void DrawCellDecorations(const GameContext* game, s8 row, s8 col, s16 x, s16 y)
{
    const GridCell* cell = &game->grid[row][col];
    /* Graves in Night (pre-placed, destroyed by Grave Buster) */
    if (cell->hasGrave)
    {
        s16 gx = x + 6;
        s16 gy = y + game->cellH - 36;
        DrawRect(gx, gy, game->cellW - 12, 26, 0x566573FF);
        DrawRect(gx + 4, gy - 8, game->cellW - 20, 8, 0x808B96FF);
    }
    /* Flower Pot on Roof (drawn under the plant) */
    if (cell->hasFlowerPot)
    {
        s16 potW = game->cellW - 12;
        s16 potH = 18;
        s16 potX = x + 6;
        s16 potY = y + game->cellH - potH - 4;
        DrawRect(potX, potY, potW, potH, 0x873600FF);
        /* Pot rim */
        DrawRect(potX - 2, potY - 3, potW + 4, 5, 0xA04000FF);
    }
    /* Lily Pad */
    if (cell->hasLilyPad)
    {
        s16 padW = game->cellW - 10;
        s16 padH = 10;
        s16 padX = x + 5;
        s16 padY = y + game->cellH - padH - 6;
        DrawRect(padX, padY, padW, padH, 0x1E8449FF);
        DrawRect(padX + 2, padY + 2, padW - 4, padH - 4, 0x27AE60FF);
    }
}
static void DrawLawnGrid(const GameContext* game)
{
    s16 lawnW = game->cellW * GRID_COLS;
    s16 lawnH = game->cellH * game->rowCount;
    DrawRect(game->lawnX, game->lawnY, lawnW, lawnH, COL_BG);
    for (s8 row = 0; row < (s8)game->rowCount; ++row)
    {
        for (s8 col = 0; col < GRID_COLS; ++col)
        {
            s16 x = game->lawnX + col * game->cellW;
            s16 y = game->lawnY + row * game->cellH;
            DrawCellBackground(game, row, col, x, y);
            DrawCellDecorations(game, row, col, x, y);
            const GridCell* cell = &game->grid[row][col];
            if (cell->plant != PLANT_NONE)
            {
                DrawPlantInCell(s_assets.plantSprite[cell->plant], x, y, game->cellW, game->cellH,
                                 game->frameCount, (u32)(row * 7 + col), PlantColor(cell->plant),
                                 cell->plant, cell->shootTimer);
                DrawHealthBar((s16)(x + game->cellW / 2), y + 4,
                               game->cellW - 8, cell->health, Plant_MaxHealth(cell->plant));
            }
            if (cell->hasPumpkin)
            {
                DrawPlantInCell(s_assets.plantSprite[PLANT_PUMPKIN], x, y, game->cellW, game->cellH,
                                 game->frameCount, (u32)(row * 13 + col * 7), 0xCC8833FF,
                                 PLANT_NONE, 0);
                DrawHealthBar((s16)(x + game->cellW / 2), y + 2,
                               game->cellW - 8, cell->pumpkinHealth, Plant_MaxHealth(PLANT_PUMPKIN));
            }
            if (cell->hasLadder)
            {
                f32 lx = (f32)x + (f32)game->cellW * 0.1f;
                f32 ly = (f32)y + 2;
                f32 lw = (f32)game->cellW * 0.8f;
                f32 lh = (f32)game->cellH - 4;
                GRRLIB_Rectangle(lx, ly, lw, lh, 0x8B5E3C60, true);
                GRRLIB_Rectangle(lx, ly, 3, lh, 0x8B5E3CFF, true);
                GRRLIB_Rectangle(lx + lw - 3, ly, 3, lh, 0x8B5E3CFF, true);
                for (f32 ry = ly + 12; ry < ly + lh - 8; ry += 16)
                    GRRLIB_Rectangle(lx, ry, lw, 3, 0x8B5E3CFF, true);
            }
        }
    }
    /* Faint grid lines */
    for (s8 col = 0; col <= GRID_COLS; ++col)
    {
        s16 x = game->lawnX + col * game->cellW;
        GRRLIB_Line((f32)x, (f32)game->lawnY, (f32)x, (f32)(game->lawnY + lawnH), COL_GRID_LINE);
    }
    for (s8 row = 0; row <= (s8)game->rowCount; ++row)
    {
        s16 y = game->lawnY + row * game->cellH;
        GRRLIB_Line((f32)game->lawnX, (f32)y, (f32)(game->lawnX + lawnW), (f32)y, COL_GRID_LINE);
    }
}
static void DrawFogOverlay(const GameContext* game)
{
    if (!(game->worldFlags & WORLD_FLAG_HAS_FOG))
        return;
    /* Fog covers the right ~40% of the lawn with a semi-transparent
     * gray that pulses slightly. In the real game it scrolls and
     * reveals as you plant. */
    s16 fogX = game->lawnX + (s16)((f32)GRID_COLS * (f32)game->cellW * 0.55f);
    s16 fogW = (s16)((f32)GRID_COLS * (f32)game->cellW * 0.45f);
    s16 fogH = (s16)((f32)game->rowCount * (f32)game->cellH);
    f32 pulse = sinf((f32)game->frameCount * 0.015f) * 0.08f;
    u8 alpha = (u8)(60 + (s8)(pulse * 60.0f));
    u32 fogColor = (0xCCCCCCu << 8) | alpha;
    DrawRect(fogX, game->lawnY, fogW, fogH, fogColor);
}
/** Outlines the tile the plant would land on if released right now, colored
 *  green when valid or red when it has been pushed off the lawn. */
static void DrawTileCursor(const GameContext* game)
{
    s16 x, y;
    Game_GridToScreen(game, game->plantCol, game->plantRow, &x, &y);
    bool valid = Game_IsValidCellForWorld(game, game->plantCol, game->plantRow);
    DrawRectOutline(x + 2, y + 2, game->cellW - 4, game->cellH - 4, valid ? COL_VALID : COL_INVALID, 2);
}
/** The seed currently held, drawn semi-transparent. With a valid IR cursor
 *  it follows the pointer directly (pixel for pixel, "picking it up" off
 *  the seed bar and carrying it); the tile-cursor outline drawn separately
 *  (DrawTileCursor) shows which square it'll actually land on. Without a
 *  valid IR cursor (fallback control scheme) it's drawn at the discrete
 *  tile-cursor position instead, since there's no pointer to follow. */
static void DrawHeldSeed(const GameContext* game, const InputState* input)
{
    if (!game->isHoldingSeed || game->heldPlant == PLANT_NONE)
        return;
    s16 w = game->cellW - 12;
    s16 h = game->cellH - 12;
    s16 x, y;
    GRRLIB_texImg* tex = s_assets.plantSprite[game->heldPlant];
    if (input && input->cursorValid)
    {
        x = (s16)input->cursor.x;
        y = (s16)input->cursor.y;
    }
    else
    {
        Game_GridToScreen(game, game->plantCol, game->plantRow, &x, &y);
        x += game->cellW / 2;
        y += game->cellH - 6;
    }
    u32 tint = 0xFFFFFF00u | (u32)HELD_SEED_ALPHA;
    if (tex && tex->data && tex->h > 0)
    {
        f32 s = (f32)h / (f32)tex->h;
        DrawSpriteQuad(tex, (f32)x, (f32)y, 0.5f, 1.0f, s, s, 0.0f, tint);
    }
    else
    {
        u32 fallback = (PlantColor(game->heldPlant) & 0xFFFFFF00u) | (u32)HELD_SEED_ALPHA;
        DrawRect(x - w / 2, y - h, w, h, fallback);
        DrawDashedBreathingBorder(x - w / 2, y - h, w, h, 0xFFDD00FF, 6, game->frameCount);
    }
}
static void DrawFallingSuns(const GameContext* game)
{
    for (u8 i = 0; i < MAX_SUN_DROPS; ++i)
    {
        const SunDrop* s = &game->suns[i];
        if (!s->active)
            continue;
        s16 size = SUN_ICON_SIZE;
        s16 x = (s16)s->x - size / 2;
        s16 y = (s16)s->y - size / 2;
        DrawTexturedBox(s_assets.sunIcon, x, y, size, size, 0xFFFFFFFF, COL_SUN_TEXT, game->frameCount);
    }
}
/** The on-screen IR reticle. Hidden whenever the Wiimote isn't currently
 *  pointed at the sensor bar, so it never looks "live" when it isn't. */
static void DrawReticle(const InputState* input)
{
    if (!input->cursorValid)
        return;
    s16 cx = (s16)input->cursor.x;
    s16 cy = (s16)input->cursor.y;
    if (s_assets.cursorIcon)
    {
        s16 size = 32;
        DrawTexturedBox(s_assets.cursorIcon, cx - size / 2, cy - size / 2, size, size, 0xFFFFFFFF, 0, 0);
        return;
    }
    DrawRectOutline(cx - CURSOR_SIZE / 2, cy - CURSOR_SIZE / 2, CURSOR_SIZE, CURSOR_SIZE, COL_CURSOR, 2);
    DrawRect(cx - 2, cy - 2, 4, 4, COL_CURSOR);
}
static void DrawHud(const GameContext* game, const InputState* input)
{
    if (!s_bitmapFont && !s_font)
        return;
    char buf[128];
    s16 y = SCREEN_HEIGHT - 20;
    snprintf(buf, sizeof(buf), "Seed: %s", Game_PlantName(SlotToPlantDisplay(game, game->selectedSlot)));
    DrawLabel(8, y, COL_HUD_TEXT, 14, buf);
    const char* mode = input->nunchukConnected ? "Nunchuk" : (input->cursorValid ? "Pointer" : "No signal");
    snprintf(buf, sizeof(buf), "Input: %s", mode);
    DrawLabel(220, y, COL_HUD_TEXT, 14, buf);
}
/* GRRLIB_LoadTTFFromFile() (GRRLIB_fileIO.c) loads the file into a buffer,
 * hands that buffer to GRRLIB_LoadTTF(), then immediately frees it. That's
 * fine for images -- GRRLIB_LoadTexture() fully decodes PNG/JPG/BMP bytes
 * into a new pixel buffer before returning, so the compressed source bytes
 * really are done with. It is NOT fine for fonts: GRRLIB_LoadTTF() calls
 * FT_New_Memory_Face(), and FreeType does not copy that memory -- it keeps
 * parsing glyph outlines directly out of the original buffer for the whole
 * life of the face. GRRLIB_LoadTTF()'s own doc comment says so explicitly:
 * "You must not deallocate the memory before calling GRRLIB_FreeTTF." The
 * *FromFile() wrapper violates that contract against its own font.
 *
 * The result is a textbook delayed-effect use-after-free: FT_New_Memory_Face
 * succeeds right away (the bytes are still intact at that instant), so
 * GRRLIB_LoadTTFFromFile() hands back a perfectly non-NULL GRRLIB_ttfFont --
 * s_font "is available". Then the freed buffer sits there and gets
 * overwritten by every texture load that follows in Render_Init() (and
 * every allocation for the rest of the game's life), so by the time any
 * DrawLabel()/Render_DrawText() call actually rasterizes a glyph, FreeType
 * is reading garbage and nothing visible comes out. And because that logic
 * always prefers s_font over s_bitmapFont whenever s_font is non-NULL, the
 * correctly-working bitmap fallback never gets a turn either, for as long
 * as a real font.ttf is present -- which it is, here (assets/font/pvz_font.ttf).
 * That's the "text invisible even though a font is available" report the
 * debug indicator above was added to chase.
 *
 * Fix: do what GRRLIB_LoadTTFFromFile() does internally, minus the premature
 * free -- keep the raw file bytes alive in s_fontFileData for as long as
 * s_font exists, and only free them in Render_Shutdown(), after
 * GRRLIB_FreeTTF(). */
static GRRLIB_ttfFont* LoadTTFKeepingBuffer(const char* path)
{
    u8* data = NULL;
    const s32 size = GRRLIB_LoadFile(path, &data);
    if (size <= 0)
        return NULL;
    GRRLIB_ttfFont* font = GRRLIB_LoadTTF(data, size);
    if (!font)
    {
        free(data);
        return NULL;
    }
    s_fontFileData = data;
    return font;
}
/* -------------------------------------------------------------------------- */
/* Public API                                                                 */
/* -------------------------------------------------------------------------- */
bool Render_Init(void)
{
    if (GRRLIB_Init() != 0)
        return false;
    GRRLIB_Settings.antialias = true;
    memset(&s_assets, 0, sizeof(s_assets));
    /* Optional font for HUD/menu text; falls back to a built-in 8x8 bitmap
     * font so the game always shows readable text even without font.ttf.
     * Uses LoadTTFKeepingBuffer() rather than GRRLIB_LoadTTFFromFile() --
     * see the comment on that function for why. */
    s_font = LoadTTFKeepingBuffer("assets/font/pvz_font.ttf");
    if (!s_font)
        s_font = LoadTTFKeepingBuffer("assets/font.ttf");
    if (!s_font)
        s_font = LoadTTFKeepingBuffer("apps/pvz_wii/assets/font.ttf");
    s_bitmapFont = BitmapFont_CreateTexture();
    if (!s_bitmapFont && !s_font)
        s_font = NULL;
    /* Loading screen: one tick per loaded group, bar + sprouts grow with it. */
    const int kLoadSteps = PLANT_TYPE_COUNT + 1 + WORLD_COUNT + ZOMBIE_TYPE_COUNT * 2 + 1;
    int loadDone = 0;
    Loading_Set(0.0f);
    /* Optional PNGs -- each load gracefully returns NULL if the file isn't
     * there yet, and every draw call falls back to a flat-colored rectangle. */
    for (int i = 0; i < PLANT_TYPE_COUNT; ++i)
    {
        s_assets.seedIcon[i] = (s_seedIconPaths[i])
            ? Render_LoadTexture(s_seedIconPaths[i])
            : NULL;
        s_assets.plantSprite[i] = (s_plantSpritePaths[i])
            ? Render_LoadTexture(s_plantSpritePaths[i])
            : NULL;
        Loading_Set((f32)(++loadDone) / (f32)kLoadSteps);
    }
    s_assets.sunIcon    = Render_LoadTexture("assets/ui/sun.png");
    s_assets.cursorIcon = Render_LoadTexture("assets/ui/cursor.png");
    s_assets.projPea    = Render_LoadTexture("assets/projectiles/pea.png");
    s_assets.pbFrame    = Render_LoadTexture("assets/ui/progress_frame.png");
    if (!s_assets.pbFrame) s_assets.pbFrame = GRRLIB_LoadTexture(kProgressArt_frame); /* embedded copy */
    s_assets.pbFill     = Render_LoadTexture("assets/ui/progress_fill.png");
    if (!s_assets.pbFill) s_assets.pbFill = GRRLIB_LoadTexture(kProgressArt_fill); /* embedded copy */
    s_assets.pbZombie   = Render_LoadTexture("assets/ui/progress_zombie.png");
    if (!s_assets.pbZombie) s_assets.pbZombie = GRRLIB_LoadTexture(kProgressArt_zombie); /* embedded copy */
    s_assets.pbFlag     = Render_LoadTexture("assets/ui/progress_flag.png");
    if (!s_assets.pbFlag) s_assets.pbFlag = GRRLIB_LoadTexture(kProgressArt_flag); /* embedded copy */
    s_assets.pbLabel    = Render_LoadTexture("assets/ui/progress_label.png");
    if (!s_assets.pbLabel) s_assets.pbLabel = GRRLIB_LoadTexture(kProgressArt_label); /* embedded copy */
    Loading_Set((f32)(++loadDone) / (f32)kLoadSteps);
    for (int i = 0; i < WORLD_COUNT; ++i)
    {
        s_assets.worldBg[i] = Render_LoadTexture(s_worldBgPaths[i]);
        Loading_Set((f32)(++loadDone) / (f32)kLoadSteps);
    }
    /* Each zombie type: try to load per-part rig textures from
     * assets/zombies/<slug>/parts/{head,torso,arm,leg_a,leg_b}.png.
     * If the parts are present, the zombie uses procedural rigged animation;
     * otherwise it falls back to the single full.png cutout below. */
    for (int i = 0; i < ZOMBIE_TYPE_COUNT; ++i)
    {
        const ZombieDef* def = Zombie_GetDef((ZombieType)i);
        char path[96];
        snprintf(path, sizeof(path), "assets/zombies/%s/parts/head.png", def->assetSlug);
        s_assets.rigHead[i] = Render_LoadTexture(path);
        snprintf(path, sizeof(path), "assets/zombies/%s/parts/torso.png", def->assetSlug);
        s_assets.rigTorso[i] = Render_LoadTexture(path);
        snprintf(path, sizeof(path), "assets/zombies/%s/parts/arm.png", def->assetSlug);
        s_assets.rigArm[i] = Render_LoadTexture(path);
        snprintf(path, sizeof(path), "assets/zombies/%s/parts/leg_a.png", def->assetSlug);
        s_assets.rigLegA[i] = Render_LoadTexture(path);
        snprintf(path, sizeof(path), "assets/zombies/%s/parts/leg_b.png", def->assetSlug);
        s_assets.rigLegB[i] = Render_LoadTexture(path);
        Loading_Set((f32)(++loadDone) / (f32)kLoadSteps);
        /* No more per-part GRRLIB_SetHandle here: positioning is now done
         * by DrawRigPart() in DrawRiggedZombie(), which always uses handle
         * (0,0) and computes xpos/ypos itself -- see the big comment above
         * that function for why. Kept trying to tune a non-zero handle by
         * eye (twice) and both times the rig came out wrong at some scale;
         * (0,0) is the one handle value with a verified, scale-independent
         * position formula. */
    }
    /* Fallback single-texture cutout for types that don't have parts/ yet.
     * Pivoted at bottom-center so the procedural sway rotates around the feet. */
    for (int i = 0; i < ZOMBIE_TYPE_COUNT; ++i)
    {
        const ZombieDef* def = Zombie_GetDef((ZombieType)i);
        char path[96];
        snprintf(path, sizeof(path), "assets/zombies/%s/full.png", def->assetSlug);
        s_assets.zombieFull[i] = Render_LoadTexture(path);
        Loading_Set((f32)(++loadDone) / (f32)kLoadSteps);
    }
    ProjFx_Load();
    Loading_Set((f32)(++loadDone) / (f32)kLoadSteps);
    Loading_Set(1.0f);
    Loading_Finish();
    return true;
}
void Render_Shutdown(void)
{
    ProjFx_Free();
    for (int i = 0; i < PLANT_TYPE_COUNT; ++i)
    {
        if (s_assets.seedIcon[i])    GRRLIB_FreeTexture(s_assets.seedIcon[i]);
        if (s_assets.plantSprite[i]) GRRLIB_FreeTexture(s_assets.plantSprite[i]);
    }
    if (s_assets.sunIcon)    GRRLIB_FreeTexture(s_assets.sunIcon);
    if (s_assets.cursorIcon) GRRLIB_FreeTexture(s_assets.cursorIcon);
    if (s_assets.projPea)    GRRLIB_FreeTexture(s_assets.projPea);
    if (s_assets.pbFrame)    GRRLIB_FreeTexture(s_assets.pbFrame);
    if (s_assets.pbFill)     GRRLIB_FreeTexture(s_assets.pbFill);
    if (s_assets.pbZombie)   GRRLIB_FreeTexture(s_assets.pbZombie);
    if (s_assets.pbFlag)     GRRLIB_FreeTexture(s_assets.pbFlag);
    if (s_assets.pbLabel)    GRRLIB_FreeTexture(s_assets.pbLabel);
    for (int i = 0; i < WORLD_COUNT; ++i)
    {
        if (s_assets.worldBg[i]) GRRLIB_FreeTexture(s_assets.worldBg[i]);
    }
    for (int i = 0; i < ZOMBIE_TYPE_COUNT; ++i)
    {
        if (s_assets.zombieFull[i]) GRRLIB_FreeTexture(s_assets.zombieFull[i]);
        if (s_assets.rigHead[i])    GRRLIB_FreeTexture(s_assets.rigHead[i]);
        if (s_assets.rigTorso[i])   GRRLIB_FreeTexture(s_assets.rigTorso[i]);
        if (s_assets.rigArm[i])     GRRLIB_FreeTexture(s_assets.rigArm[i]);
        if (s_assets.rigLegA[i])    GRRLIB_FreeTexture(s_assets.rigLegA[i]);
        if (s_assets.rigLegB[i])    GRRLIB_FreeTexture(s_assets.rigLegB[i]);
    }
    memset(&s_assets, 0, sizeof(s_assets));
    if (s_font)
    {
        GRRLIB_FreeTTF(s_font);
        s_font = NULL;
    }
    /* Only safe to free now that FreeType is done with it -- see
     * LoadTTFKeepingBuffer()'s comment. */
    if (s_fontFileData)
    {
        free(s_fontFileData);
        s_fontFileData = NULL;
    }
    if (s_bitmapFont)
    {
        GRRLIB_FreeTexture(s_bitmapFont);
        s_bitmapFont = NULL;
    }
    GRRLIB_Exit();
}
GRRLIB_ttfFont* Render_GetFont(void)
{
    return s_font;
}
GRRLIB_texImg* Render_GetBitmapFont(void)
{
    return s_bitmapFont;
}
void Render_DrawText(s16 x, s16 y, u32 color, unsigned int size, const char* text)
{
    if (!text) return;
    if (s_font)
        GRRLIB_PrintfTTF(x, y, s_font, text, size, color);
    else if (s_bitmapFont)
        GRRLIB_Printf((f32)x, (f32)y, s_bitmapFont, color, BitmapFontZoomForSize(size), "%s", text);
}

int Render_TextWidth(unsigned int size, const char* text)
{
    if (!text) return 0;
    if (s_font)
        return (int)GRRLIB_WidthTTF(s_font, text, size);
    return (int)(strlen(text) * size); /* bitmap font: 8px glyph * (size/8) zoom */
}

void Render_DrawTextCentered(s16 cx, s16 y, u32 color, unsigned int size, const char* text)
{
    if (!text) return;
    Render_DrawText((s16)(cx - Render_TextWidth(size, text) / 2), y, color, size, text);
}

int Render_DrawTextWrapped(s16 x, s16 y, s16 maxWidth, s16 lineHeight, u32 color,
                           unsigned int size, const char* text)
{
    if (!text) return 0;
    char line[160];
    line[0] = '\0';
    int lines = 0;
    const char* p = text;
    while (*p)
    {
        while (*p == ' ') ++p;
        const char* w = p;
        while (*p && *p != ' ') ++p;
        char word[64];
        size_t wl = (size_t)(p - w);
        if (wl == 0) break;
        if (wl >= sizeof(word)) wl = sizeof(word) - 1;
        memcpy(word, w, wl);
        word[wl] = '\0';

        char trial[224];
        if (line[0])
            snprintf(trial, sizeof(trial), "%s %s", line, word);
        else
            snprintf(trial, sizeof(trial), "%s", word);

        if (line[0] && Render_TextWidth(size, trial) > maxWidth)
        {
            Render_DrawText(x, (s16)(y + lines * lineHeight), color, size, line);
            ++lines;
            snprintf(line, sizeof(line), "%s", word);
        }
        else
        {
            snprintf(line, sizeof(line), "%s", trial);
        }
    }
    if (line[0])
    {
        Render_DrawText(x, (s16)(y + lines * lineHeight), color, size, line);
        ++lines;
    }
    return lines;
}

void Render_DrawPlantPortrait(PlantType type, s16 cx, s16 bottomY, s16 targetH, s16 maxW,
                              u32 frameCount)
{
    if (type < 0 || type >= PLANT_TYPE_COUNT) return;
    GRRLIB_texImg* tex = s_assets.plantSprite[type];
    if (!tex || !tex->data) tex = s_assets.seedIcon[type];
    if (!tex || !tex->data || tex->h <= 0 || tex->w <= 0) return;
    f32 s = (f32)targetH / (f32)tex->h;
    if ((f32)tex->w * s > (f32)maxW) s = (f32)maxW / (f32)tex->w;
    f32 t = (f32)frameCount * 0.05f;
    DrawSpriteQuad(tex, (f32)cx, (f32)bottomY - fabsf(sinf(t * 0.5f)) * 3.0f, 0.5f, 1.0f,
                   s, s, sinf(t) * 3.0f, 0xFFFFFFFF);
}

void Render_DrawReticle(const InputState* input)
{
    DrawReticle(input);
}
/** "Zombies incoming" gauge, bottom-center: fills up over the next
 *  ZOMBIE_SPAWN_INTERVAL_FRAMES-ish and resets when a zombie actually
 *  spawns. Hidden when there's nothing left to spawn right now (mid-wave
 *  cleanup, level-won/over intermissions). */
static void DrawSpawnGauge(const GameContext* game)
{
    f32 progress = Level_GetSpawnGaugeProgress(game);
    if (progress < 0.0f) return;
    s16 x = (SCREEN_WIDTH - SPAWN_GAUGE_WIDTH) / 2;
    s16 y = SCREEN_HEIGHT - SPAWN_GAUGE_HEIGHT - SPAWN_GAUGE_MARGIN_Y;
    DrawRect(x, y, SPAWN_GAUGE_WIDTH, SPAWN_GAUGE_HEIGHT, 0x00000090);
    s16 fillW = (s16)((f32)(SPAWN_GAUGE_WIDTH - 4) * progress);
    if (fillW > 0)
        DrawRect(x + 2, y + 2, fillW, SPAWN_GAUGE_HEIGHT - 4, 0xDD444400 | 0xFF);
    DrawRectOutline(x, y, SPAWN_GAUGE_WIDTH, SPAWN_GAUGE_HEIGHT, COL_HUD_TEXT, 1);
    DrawLabel(x, y - 16, COL_HUD_TEXT, 12, "Zombies incoming...");
}
/** Right-side level progress bar: overall progress through the current
 *  level's wave set, with a thin tick mark at each wave boundary. Draws the
 *  eased game->levelProgressDisplayed value (see Game_Update), which is what
 *  gives it the visible slow-down-and-settle motion at each tick instead of
 *  a linear/instant fill. Hidden outside PLAYING/LEVEL_WON (selection
 *  screen, game over) or if there's no wave data yet. */
static void DrawLevelProgressBarSimple(const GameContext* game)
{
    if (game->state != GAME_STATE_PLAYING && game->state != GAME_STATE_LEVEL_WON) return;
    if (game->waveCount == 0) return;

    s16 w = LEVEL_PROGRESS_BAR_WIDTH;
    s16 h = LEVEL_PROGRESS_BAR_HEIGHT;
    s16 x = SCREEN_WIDTH - w - LEVEL_PROGRESS_BAR_MARGIN_X;
    s16 y = LEVEL_PROGRESS_BAR_MARGIN_Y;

    f32 progress = game->levelProgressDisplayed;
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;

    DrawRect(x, y, w, h, 0x00000090);
    s16 fillW = (s16)((f32)(w - 4) * progress);
    if (fillW > 0)
        DrawRect(x + 2, y + 2, fillW, h - 4, 0x33CC33FF);
    DrawRectOutline(x, y, w, h, COL_HUD_TEXT, 1);

    /* One tick per wave boundary. */
    f32 tickFractions[MAX_WAVES_PER_LEVEL];
    u8 tickCount = Level_GetWaveTickFractions(game, tickFractions);
    for (u8 i = 0; i < tickCount; ++i)
    {
        s16 tx = x + (s16)((f32)w * tickFractions[i]);
        if (tx <= x) tx = x + 1;
        if (tx >= x + w) tx = x + w - 1;
        GRRLIB_Line((f32)tx, (f32)y, (f32)tx, (f32)(y + h), 0x000000C0);
    }
}
/** Level-progress meter, bottom-right: a framed bar that fills from the right
 *  (where the zombies come from) toward the left as the level advances, a
 *  zombie head riding the front of the fill, a flag at the start of every
 *  wave (it pops up once that point has been reached), and the
 *  "LEVEL PROGRESS" caption underneath. Uses the eased
 *  game->levelProgressDisplayed value. Falls back to the plain bar if the
 *  art in assets/ui/progress_*.png is missing. */
static void DrawLevelProgressBar(const GameContext* game)
{
    GRRLIB_texImg* frame = s_assets.pbFrame;
    GRRLIB_texImg* fill  = s_assets.pbFill;
    if (!frame || !frame->data || !fill || !fill->data)
    {
        DrawLevelProgressBarSimple(game);
        return;
    }
    if (game->state != GAME_STATE_PLAYING && game->state != GAME_STATE_LEVEL_WON) return;
    if (game->waveCount == 0) return;

    f32 progress = game->levelProgressDisplayed;
    if (progress < 0.0f) progress = 0.0f;
    if (progress > 1.0f) progress = 1.0f;

    const f32 fw = (f32)frame->w, fh = (f32)frame->h;
    const f32 fx = (f32)(SCREEN_WIDTH - frame->w - 14);
    const f32 fy = (f32)(SCREEN_HEIGHT - 12 - 3 - frame->h - 14);

    /* Geometry of the green well inside the frame art (see assets/ui/progress_*.png):
     * the fill strip's top-left sits at (8,8) and it is 144 px wide when full. */
    const f32 wellW    = (f32)fill->w;
    const f32 wellRight = fx + 8.0f + wellW;
    const f32 wellTop   = fy + 8.0f;

    DrawSpriteQuad(frame, fx, fy, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0xFFFFFFFF);

    f32 fillW = wellW * progress;
    if (fillW >= 1.0f)
        DrawSpriteQuad(fill, wellRight - fillW, wellTop, 0.0f, 0.0f, progress, 1.0f, 0.0f, 0xFFFFFFFF);

    /* Wave flags: planted at the start of each wave; raised once reached. */
    GRRLIB_texImg* flag = s_assets.pbFlag;
    if (flag && flag->data)
    {
        f32 ticks[MAX_WAVES_PER_LEVEL];
        u8 tickCount = Level_GetWaveTickFractions(game, ticks);
        for (u8 i = 0; i < tickCount; ++i)
        {
            f32 tx = wellRight - (wellW - 1.0f) * ticks[i];
            bool reached = progress >= ticks[i];
            f32 baseY = fy + 4.0f - (reached ? 5.0f : 0.0f);   /* flag's bottom edge */
            DrawSpriteQuad(flag, tx, baseY, 0.1f, 1.0f, 1.0f, 1.0f, 0.0f, 0xFFFFFFFF);
        }
    }

    /* Zombie head at the front of the fill */
    GRRLIB_texImg* head = s_assets.pbZombie;
    if (head && head->data)
    {
        f32 hx = wellRight - fillW;
        f32 wob = sinf((f32)game->frameCount * 0.12f) * 1.0f;
        DrawSpriteQuad(head, hx, fy + fh * 0.5f + 5.0f + wob, 0.5f, 1.0f, 1.0f, 1.0f, 0.0f, 0xFFFFFFFF);
    }

    /* Caption below */
    GRRLIB_texImg* label = s_assets.pbLabel;
    if (label && label->data)
        DrawSpriteQuad(label, fx + (fw - (f32)label->w) * 0.5f, fy + fh + 2.0f, 0.0f, 0.0f, 1.0f, 1.0f, 0.0f, 0xFFFFFFFF);
}
static void DrawSelectionScreen(const GameContext* game, const InputState* input)
{
    if (game->state != GAME_STATE_SELECTING)
        return;
    DrawWorldBackground(game);
    DrawLawnGrid(game);
    DrawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0x000000B0);
    DrawLabel(SCREEN_WIDTH / 2 - 80, 20, COL_HUD_TEXT, 18, "Choose your plants");
    s16 iconSize = 52;
    s16 cols = 5;
    s16 gap = 8;
    s16 totalW = cols * iconSize + (cols - 1) * gap;
    s16 startX = (SCREEN_WIDTH - totalW) / 2;
    s16 startY = 60;
    for (u8 i = 0; i < game->unlockedCount; ++i)
    {
        s16 ix = startX + (i % cols) * (iconSize + gap);
        s16 iy = startY + (i / cols) * (iconSize + gap + 16);
        PlantType type = game->unlockedPlants[i];
        u32 color = PlantColor(type);
        bool isSelected = false;
        for (u8 s = 0; s < game->selectedCount; ++s)
            if (game->selectedPlants[s] == type) { isSelected = true; break; }
        bool isCursor = (s8)i == game->selectionCursor;
        /* Background */
        DrawRect(ix, iy, iconSize, iconSize, isSelected ? 0x33AA33A0 : 0x333333A0);
        if (isSelected)
            DrawRectOutline(ix, iy, iconSize, iconSize, 0x44FF44FF, 2);
        /* Icon placeholder */
        DrawTexturedBox(s_assets.seedIcon[type], ix + 2, iy + 2, iconSize - 4, iconSize - 4,
                        0xFFFFFFFF, color, game->frameCount);
        /* Name below */
        if (s_bitmapFont || s_font)
        {
            const char* name = Game_PlantName(type);
            s16 labelW = (s16)(strlen(name) * 7);
            s16 lx = ix + (iconSize - labelW) / 2;
            if (lx < 0) lx = 0;
            DrawLabel(lx, iy + iconSize + 2, COL_HUD_TEXT, 10, name);
        }
        /* Cursor highlight */
        if (isCursor)
            DrawRectOutline(ix - 3, iy - 3, iconSize + 6, iconSize + 6, 0xFFDD33FF, 3);
    }
    /* Selected list on the right side */
    s16 listX = SCREEN_WIDTH - 150;
    s16 listY = 60;
    DrawLabel(listX, listY - 14, COL_SUN_TEXT, 12, "Selected:");
    for (u8 i = 0; i < game->selectedCount; ++i)
    {
        DrawTexturedBox(s_assets.seedIcon[game->selectedPlants[i]],
                        listX, listY + i * 30, 24, 24,
                        0xFFFFFFFF, PlantColor(game->selectedPlants[i]), game->frameCount);
        DrawLabel(listX + 28, listY + i * 30 + 6, COL_HUD_TEXT, 10,
                  Game_PlantName(game->selectedPlants[i]));
    }
    /* Instructions */
    if (game->isSurvivalReselect)
    {
        char buf[48];
        snprintf(buf, sizeof(buf), "Survival: Flag %u  |  A: select  |  +: next wave", (unsigned)game->survivalFlags);
        DrawLabel(SCREEN_WIDTH / 2 - 140, SCREEN_HEIGHT - 30, COL_SUN_TEXT, 12, buf);
    }
    else
    {
        DrawLabel(SCREEN_WIDTH / 2 - 100, SCREEN_HEIGHT - 30, COL_SUN_TEXT, 12,
                  "A: select/deselect  |  +: begin");
    }
}
/** Fill color per projectile kind, and (for the 4 pea-shaped kinds) the tint
 *  applied to the shared assets/projectiles/pea.png sprite -- see
 *  DrawProjectiles. The other kinds still draw as a plain circle, since
 *  that's still what reads as "a cabbage", "a corn kernel", etc. without a
 *  modeled sprite for each. The classic Peashooter/Repeater/Gatling
 *  Pea/Threepeater/Split Pea shot is PROJ_PEA -- the plain green pea ball
 *  from the original game. */
static u32 ProjectileColor(ProjectileKind kind)
{
    switch (kind)
    {
        case PROJ_PEA:     return 0x4CAF32FF; /* classic green pea           */
        case PROJ_SNOW:    return 0xAEEFFFFF; /* Snow Pea's icy pea          */
        case PROJ_FIRE:    return 0xFF7A1AFF; /* Torchwood-buffed pea        */
        case PROJ_FUME:    return 0x9ACD32FF; /* Fume-shroom / Gloom-shroom  */
        case PROJ_SPORE:   return 0xE6D6FFFF; /* Puff/Sea-shroom spore       */
        case PROJ_STAR:    return 0xFFD84DFF; /* Starfruit                   */
        case PROJ_CABBAGE: return 0x4C9A2AFF; /* Cabbage-pult                */
        case PROJ_KERNEL:  return 0xFFD34DFF; /* Kernel-pult corn            */
        case PROJ_BUTTER:  return 0xFFF3B0FF; /* Kernel-pult butter          */
        case PROJ_MELON:   return 0x3FAF4FFF; /* Melon-pult                  */
        case PROJ_WINTER:  return 0xA0E8FFFF; /* Winter Melon                */
        case PROJ_SPIKE:   return 0x6B4A2FFF; /* Spikeweed/Spikerock/Cactus  */
        case PROJ_HOMING:  return 0xC9B8A0FF; /* Cattail spike               */
        default:           return 0x4CAF32FF;
    }
}
/** Radius in pixels for each projectile kind's circle. */
static f32 ProjectileRadius(ProjectileKind kind)
{
    switch (kind)
    {
        case PROJ_SPORE:                 return 4.0f;
        case PROJ_CABBAGE:
        case PROJ_MELON:
        case PROJ_WINTER:                return 9.0f;
        case PROJ_KERNEL:
        case PROJ_BUTTER:                return 5.0f;
        case PROJ_SPIKE:                 return 4.0f;
        default:                         return 6.0f; /* PROJ_PEA and co. */
    }
}
static void DrawProjectiles(const GameContext* game)
{
    ProjFx_BeginFrame(game); /* hit detection -> impact effects */
    for (int i = 0; i < MAX_PROJECTILES; i++)
    {
        const Projectile* p = &game->projectiles[i];
        if (!p->active) continue;
        f32 sx = game->lawnX + p->x * game->cellW;
        f32 sy = game->lawnY + p->y * game->cellH;
        if (ProjFx_DrawProjectile(game, i, sx, sy)) continue; /* real sprite drawn */
        /* else: art missing -> old tinted pea / circle placeholder below */
        bool isPeaShaped = (p->kind == PROJ_PEA || p->kind == PROJ_SNOW ||
                            p->kind == PROJ_FIRE || p->kind == PROJ_FUME);
        if (isPeaShaped && s_assets.projPea && s_assets.projPea->data)
        {
            f32 d = ProjectileRadius(p->kind) * 2.0f;
            DrawTexturedBox(s_assets.projPea, (s16)(sx - d / 2.0f), (s16)(sy - d / 2.0f),
                            (s16)d, (s16)d, ProjectileColor(p->kind), ProjectileColor(p->kind),
                            game->frameCount);
        }
        else
        {
            GRRLIB_Circle(sx, sy, ProjectileRadius(p->kind), ProjectileColor(p->kind), true);
        }
    }
    ProjFx_DrawEffects(game);
}
static void DrawParticles(const GameContext* game)
{
    for (int i = 0; i < MAX_PARTICLES; i++)
    {
        const Particle* p = &game->particles[i];
        if (!p->active) continue;
        f32 sx = game->lawnX + p->x * game->cellW;
        f32 sy = game->lawnY + p->y * game->cellH;
        f32 sz = p->size;
        GRRLIB_Rectangle(sx - sz * 0.5f, sy - sz * 0.5f, sz, sz, p->color, true);
    }
}
void Render_Frame(const GameContext* game, const InputState* input)
{
    if (game->state == GAME_STATE_SELECTING)
    {
        DrawSelectionScreen(game, input);
        DrawReticle(input);
        DrawLevelBanner(game);
        return;
    }
    DrawWorldBackground(game);
    DrawLawnGrid(game);
    DrawZombies(game);
    DrawProjectiles(game);
    DrawParticles(game);
    DrawFallingSuns(game);
    DrawFogOverlay(game);
    if (!input->cursorValid || game->isHoldingSeed)
        DrawTileCursor(game);
    DrawHeldSeed(game, input);
    DrawSeedBar(game, input);
    DrawHud(game, input);
    DrawLevelBanner(game);
    DrawLevelProgressBar(game);
    DrawSpawnGauge(game);
    DrawReticle(input);
}
void Render_Present(void)
{
    GRRLIB_Render();
}
