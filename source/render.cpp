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

#include <grrlib.h>
#include <stdio.h>
#include <math.h>
#include <string.h>
#include <math.h>

/* -------------------------------------------------------------------------- */
/* Placeholder color palette (used whenever the matching PNG is absent)      */
/* -------------------------------------------------------------------------- */

static const u32 COL_BG        = 0x2D5A1EFF;
static const u32 COL_LAWN_DARK = 0x1E4514FF;
static const u32 COL_GRID_LINE = 0x00000033;
static const u32 COL_SEED_BAR  = 0x5C4033FF;
static const u32 COL_SLOT      = 0x8B6914FF;
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
} RenderAssets;

static RenderAssets     s_assets;
static GRRLIB_ttfFont*  s_font = NULL;
static GRRLIB_texImg*   s_bitmapFont = NULL;

static const char* s_seedIconPaths[PLANT_TYPE_COUNT] = {
    "assets/seeds/peashooter.png",
    "assets/seeds/sunflower.png",
    "assets/seeds/wallnut.png",
    "assets/seeds/cherrybomb.png",
    "assets/seeds/repeater.png",
    "assets/seeds/snowpea.png",
};

static const char* s_plantSpritePaths[PLANT_TYPE_COUNT] = {
    "assets/plants/peashooter.png",
    "assets/plants/sunflower.png",
    "assets/plants/wallnut.png",
    "assets/plants/cherrybomb.png",
    "assets/plants/repeater.png",
    "assets/plants/snowpea.png",
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
    if (s_font)
        GRRLIB_PrintfTTF(x, y, s_font, text, size, color);
    else if (s_bitmapFont)
        GRRLIB_Printf((f32)x, (f32)y, s_bitmapFont, color, BitmapFontZoomForSize(size), "%s", text);
}

/** Draws `tex` scaled to fit (w,h) at (x,y) if loaded; otherwise a flat rect. */
static void DrawTexturedBox(GRRLIB_texImg* tex, s16 x, s16 y, s16 w, s16 h, u32 tint, u32 fallbackColor, u32 frameCount)
{
    if (tex && tex->data)
    {
        /* Defensive: some textures (planted plants, zombies) get a
         * bottom/top-center pivot set elsewhere via GRRLIB_SetHandle for
         * their sway/walk animation. This helper always means "draw at
         * (x,y) top-left", so it resets the pivot before every draw rather
         * than assuming nobody else touched it. */
        GRRLIB_SetHandle(tex, 0, 0);
        f32 scaleX = (tex->w > 0) ? ((f32)w / (f32)tex->w) : 1.0f;
        f32 scaleY = (tex->h > 0) ? ((f32)h / (f32)tex->h) : 1.0f;
        GRRLIB_DrawImg((f32)x, (f32)y, tex, 0.0f, scaleX, scaleY, tint);
    }
    else
    {
        DrawRect(x, y, w, h, fallbackColor);
        DrawDashedBreathingBorder(x, y, w, h, 0xFFDD00FF, 6, frameCount);
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
static void DrawPlantInCell(GRRLIB_texImg* tex, s16 cellX, s16 cellY, s16 cellW, s16 cellH,
                            u32 frameCount, u32 phaseSeed, u32 fallbackColor)
{
    s16 pad = 6;
    if (tex && tex->data && tex->h > 0)
    {
        /* Set (not just rely on load-time) since DrawTexturedBox -- used for
         * the held-seed preview on this same texture -- resets the pivot to
         * (0,0) later in the frame; each draw call owns the pivot it needs. */
        GRRLIB_SetHandle(tex, tex->w / 2, tex->h);

        f32 footX = (f32)(cellX + cellW / 2);
        f32 footY = (f32)(cellY + cellH - pad);
        f32 phase = (f32)frameCount * 0.045f + (f32)(phaseSeed % 7) * 0.9f;
        f32 sway  = sinf(phase) * 3.0f;
        f32 bob   = fabsf(sinf(phase * 0.5f)) * 2.0f;
        f32 targetH = (f32)(cellH - pad * 2);
        f32 s = targetH / (f32)tex->h;
        GRRLIB_DrawImg(footX, footY - bob, tex, sway, s, s, 0xFFFFFFFF);
    }
    else
    {
        DrawRect(cellX + pad, cellY + pad, cellW - pad * 2, cellH - pad * 2, fallbackColor);
        DrawDashedBreathingBorder(cellX + pad, cellY + pad, cellW - pad * 2, cellH - pad * 2, 0xFFDD00FF, 6, frameCount);
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
        GRRLIB_DrawImg(footX, footY - bob, tex, sway, s, s, color);
    }
    else
    {
        f32 w = game->cellW * 0.7f;
        DrawRect((s16)(footX - w / 2), (s16)(footY - targetH), (s16)w, (s16)targetH, 0x336633FF);
    }
}

/** All zombie types: composited from per-type parts/ (head, torso, arm,
 *  leg_a, leg_b) and animated frame-by-frame with procedural walk-cycle
 *  limb swinging. Each zombie type now provides its own parts, so the
 *  accessory system (cone/bucket/flag/helmet) is no longer needed --
 *  each type's head.png already includes its decoration. */
static void DrawRiggedZombie(const GameContext* game, const Zombie* z, const ZombieDef* def)
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
    f32 hipY   = footY - legH * 0.92f - bodyBob;

    if (legB) GRRLIB_DrawImg(footX - 6, hipY, legB, legBAngle, legScale, legScale, color);
    if (legA) GRRLIB_DrawImg(footX + 6, hipY, legA, legAAngle, legScale, legScale, color);

    f32 torsoH = torso ? torso->h * legScale : game->cellH * 0.7f;
    f32 shoulderY = hipY - torsoH * 0.85f;

    if (arm)
    {
        GRRLIB_DrawImg(footX - 10, shoulderY, arm,  armAngle, -legScale, legScale, color);
        GRRLIB_DrawImg(footX + 10, shoulderY, arm, -armAngle,  legScale, legScale, color);
    }
    if (torso) GRRLIB_DrawImg(footX, shoulderY, torso, 0.0f, legScale, legScale, color);

    f32 headY = shoulderY - torsoH * 0.62f;
    if (head) GRRLIB_DrawImg(footX, headY, head, 0.0f, legScale, legScale, color);
}

static void DrawZombieHealth(const GameContext* game, const Zombie* z)
{
    f32 footX = TileToScreenX(game, z->x + 0.5f);
    f32 footY = RowToScreenBottomY(game, z->row);
    const ZombieDef* def = Zombie_GetDef(z->type);
    DrawHealthBar((s16)footX, (s16)(footY - game->cellH * 1.6f - 2),
                  game->cellW - 8, z->health, def->health);
}

static void DrawZombies(const GameContext* game)
{
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

        if (s_assets.rigHead[z->type] && s_assets.rigHead[z->type]->data)
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
    if (!s_font) return;

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
}


/* -------------------------------------------------------------------------- */
/* Frame sections                                                             */
/* -------------------------------------------------------------------------- */

static void DrawSeedBar(const GameContext* game, const InputState* input)
{
    DrawRect(0, 0, SCREEN_WIDTH, SEED_BAR_HEIGHT, COL_SEED_BAR);

    s16 slotY = (SEED_BAR_HEIGHT - SEED_SLOT_HEIGHT) / 2;

    /* Sun counter, left-aligned in the seed bar */
    s16 sunX = SEED_BAR_PADDING + 4;
    s16 sunY = (SEED_BAR_HEIGHT - 40) / 2;
    DrawTexturedBox(s_assets.sunIcon, sunX, sunY, 40, 40, 0xFFFFFFFF, COL_SUN_TEXT, game->frameCount);
    if (s_font)
    {
        char sunBuf[8];
        snprintf(sunBuf, sizeof(sunBuf), "%u", (unsigned)game->sun);
        DrawLabel(sunX + 44, sunY + 12, COL_HUD_TEXT, 16, sunBuf);
    }

    u8 slots = game->selectedCount;
    if (slots > SEED_SLOT_COUNT) slots = SEED_SLOT_COUNT;

    for (s8 i = 0; i < (s8)slots; ++i)
    {
        s16 slotX = SEED_BAR_PADDING + 64 + i * (SEED_SLOT_WIDTH + SEED_BAR_PADDING);

        PlantType type = SlotToPlantDisplay(game, i);
        if (type < 0) continue;

        bool isSelected = (i == game->selectedSlot);
        bool isHovered  = (i == input->hoveredSeedSlot);
        u32  slotColor  = isSelected ? COL_SLOT_SEL : (isHovered ? COL_SLOT_HOVER : COL_SLOT);

        DrawRect(slotX, slotY, SEED_SLOT_WIDTH, SEED_SLOT_HEIGHT, 0x1A1A1AFF);
        DrawDashedBreathingBorder(slotX, slotY, SEED_SLOT_WIDTH, SEED_SLOT_HEIGHT, slotColor, isSelected ? 10 : 4, game->frameCount);

        const s16 iconSize = 48;
        s16 iconX = slotX + (SEED_SLOT_WIDTH - iconSize) / 2;
        s16 iconY = slotY + 3;

        DrawTexturedBox(s_assets.seedIcon[type], iconX, iconY, iconSize, iconSize, 0xFFFFFFFF, PlantColor(type), game->frameCount);

        u16 cost = Game_PlantCost(type);
        bool affordable = game->sun >= cost;
        if (!affordable)
            DrawRect(slotX, slotY, SEED_SLOT_WIDTH, SEED_SLOT_HEIGHT, 0x00000090);

        if (s_font)
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
                                 game->frameCount, (u32)(row * 7 + col), PlantColor(cell->plant));
                DrawHealthBar((s16)(x + game->cellW / 2), y + 4,
                               game->cellW - 8, cell->health, Plant_MaxHealth(cell->plant));
            }
            if (cell->hasPumpkin)
            {
                DrawPlantInCell(s_assets.plantSprite[PLANT_PUMPKIN], x, y, game->cellW, game->cellH,
                                 game->frameCount, (u32)(row * 13 + col * 7), 0xCC8833FF);
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

    if (input && input->cursorValid)
    {
        x = (s16)input->cursor.x - w / 2;
        y = (s16)input->cursor.y - h / 2;
    }
    else
    {
        s16 tileX, tileY;
        Game_GridToScreen(game, game->plantCol, game->plantRow, &tileX, &tileY);
        x = tileX + 6;
        y = tileY + 6;
    }

    u32 tint     = 0xFFFFFF00u | (u32)HELD_SEED_ALPHA;
    u32 fallback = (PlantColor(game->heldPlant) & 0xFFFFFF00u) | (u32)HELD_SEED_ALPHA;

    DrawTexturedBox(s_assets.plantSprite[game->heldPlant], x, y, w, h, tint, fallback, game->frameCount);
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
    if (!s_font)
        return;

    char buf[64];
    s16 y = SCREEN_HEIGHT - 20;

    snprintf(buf, sizeof(buf), "Seed: %s", Game_PlantName(SlotToPlantDisplay(game, game->selectedSlot)));
    DrawLabel(8, y, COL_HUD_TEXT, 14, buf);

    const char* mode = input->nunchukConnected ? "Nunchuk" : (input->cursorValid ? "Pointer" : "No signal");
    snprintf(buf, sizeof(buf), "Input: %s", mode);
    DrawLabel(220, y, COL_HUD_TEXT, 14, buf);

    /* [DEBUG] Wave/spawn state -- delete this block once zombies are
     * confirmed to be spawning correctly; it's here purely to make that
     * visible from the TV without needing build logs. */
    if (game->state == GAME_STATE_PLAYING)
    {
        int active = 0;
        for (int i = 0; i < MAX_ZOMBIES; ++i)
            if (game->zombies[i].active) active++;

        const LevelWave* wave = &game->waves[game->currentWave];
        snprintf(buf, sizeof(buf), "[DBG] Wave %u/%u  spawned %u/%u  active %d  next %.1fs",
                 (unsigned)(game->currentWave + 1), (unsigned)game->waveCount,
                 (unsigned)wave->spawnedCount, (unsigned)wave->zombieCount,
                 active, (double)wave->nextSpawnTimer / (double)TARGET_FPS);
        DrawLabel(8, y - 18, 0xFFFF00FF, 12, buf);
    }
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
     * font so the game always shows readable text even without font.ttf. */
    s_font = GRRLIB_LoadTTFFromFile("assets/font.ttf");
    if (!s_font)
        s_font = GRRLIB_LoadTTFFromFile("apps/pvz_wii/assets/font.ttf");
    s_bitmapFont = NULL;
    if (!s_font)
        s_bitmapFont = BitmapFont_CreateTexture();

    /* Optional PNGs -- each load gracefully returns NULL if the file isn't
     * there yet, and every draw call falls back to a flat-colored rectangle. */
    for (int i = 0; i < PLANT_TYPE_COUNT; ++i)
    {
        s_assets.seedIcon[i] = (s_seedIconPaths[i])
            ? GRRLIB_LoadTextureFromFile(s_seedIconPaths[i])
            : NULL;

        s_assets.plantSprite[i] = (s_plantSpritePaths[i])
            ? GRRLIB_LoadTextureFromFile(s_plantSpritePaths[i])
            : NULL;
        if (s_assets.plantSprite[i])
        {
            GRRLIB_SetHandle(s_assets.plantSprite[i], s_assets.plantSprite[i]->w / 2, s_assets.plantSprite[i]->h);
        }
    }
    s_assets.sunIcon    = GRRLIB_LoadTextureFromFile("assets/ui/sun.png");
    s_assets.cursorIcon = GRRLIB_LoadTextureFromFile("assets/ui/cursor.png");

    for (int i = 0; i < WORLD_COUNT; ++i)
    {
        s_assets.worldBg[i] = GRRLIB_LoadTextureFromFile(s_worldBgPaths[i]);
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
        s_assets.rigHead[i] = GRRLIB_LoadTextureFromFile(path);
        snprintf(path, sizeof(path), "assets/zombies/%s/parts/torso.png", def->assetSlug);
        s_assets.rigTorso[i] = GRRLIB_LoadTextureFromFile(path);
        snprintf(path, sizeof(path), "assets/zombies/%s/parts/arm.png", def->assetSlug);
        s_assets.rigArm[i] = GRRLIB_LoadTextureFromFile(path);
        snprintf(path, sizeof(path), "assets/zombies/%s/parts/leg_a.png", def->assetSlug);
        s_assets.rigLegA[i] = GRRLIB_LoadTextureFromFile(path);
        snprintf(path, sizeof(path), "assets/zombies/%s/parts/leg_b.png", def->assetSlug);
        s_assets.rigLegB[i] = GRRLIB_LoadTextureFromFile(path);

        /* Set pivots: head at center-X 3/4 down, torso at center-X top,
         * limbs at center-X near top (shoulder/hip joint). */
        if (s_assets.rigHead[i])  GRRLIB_SetHandle(s_assets.rigHead[i],  s_assets.rigHead[i]->w / 2, s_assets.rigHead[i]->h * 3 / 4);
        if (s_assets.rigTorso[i]) GRRLIB_SetHandle(s_assets.rigTorso[i], s_assets.rigTorso[i]->w / 2, 4);
        if (s_assets.rigArm[i])   GRRLIB_SetHandle(s_assets.rigArm[i],   s_assets.rigArm[i]->w / 2, 8);
        if (s_assets.rigLegA[i])  GRRLIB_SetHandle(s_assets.rigLegA[i],  s_assets.rigLegA[i]->w / 2, 8);
        if (s_assets.rigLegB[i])  GRRLIB_SetHandle(s_assets.rigLegB[i],  s_assets.rigLegB[i]->w / 2, 8);
    }

    /* Fallback single-texture cutout for types that don't have parts/ yet.
     * Pivoted at bottom-center so the procedural sway rotates around the feet. */
    for (int i = 0; i < ZOMBIE_TYPE_COUNT; ++i)
    {
        const ZombieDef* def = Zombie_GetDef((ZombieType)i);
        char path[96];
        snprintf(path, sizeof(path), "assets/zombies/%s/full.png", def->assetSlug);
        s_assets.zombieFull[i] = GRRLIB_LoadTextureFromFile(path);
        if (s_assets.zombieFull[i])
        {
            GRRLIB_SetHandle(s_assets.zombieFull[i], s_assets.zombieFull[i]->w / 2, s_assets.zombieFull[i]->h);
        }
    }

    return true;
}

void Render_Shutdown(void)
{
    for (int i = 0; i < PLANT_TYPE_COUNT; ++i)
    {
        if (s_assets.seedIcon[i])    GRRLIB_FreeTexture(s_assets.seedIcon[i]);
        if (s_assets.plantSprite[i]) GRRLIB_FreeTexture(s_assets.plantSprite[i]);
    }
    if (s_assets.sunIcon)    GRRLIB_FreeTexture(s_assets.sunIcon);
    if (s_assets.cursorIcon) GRRLIB_FreeTexture(s_assets.cursorIcon);

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

static void DrawSelectionScreen(const GameContext* game, const InputState* input)
{
    if (game->state != GAME_STATE_SELECTING)
        return;

    DrawRect(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0x000000E0);

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
        if (s_font)
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
        snprintf(buf, sizeof(buf), "Survival: Flag %u  |  A: select  |  START: next wave", (unsigned)game->survivalFlags);
        DrawLabel(SCREEN_WIDTH / 2 - 140, SCREEN_HEIGHT - 30, COL_SUN_TEXT, 12, buf);
    }
    else
    {
        DrawLabel(SCREEN_WIDTH / 2 - 100, SCREEN_HEIGHT - 30, COL_SUN_TEXT, 12,
                  "A: select/deselect  |  START: begin");
    }
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
    DrawParticles(game);
    DrawFallingSuns(game);
    DrawFogOverlay(game);
    if (!input->cursorValid || game->isHoldingSeed)
        DrawTileCursor(game);
    DrawHeldSeed(game, input);
    DrawSeedBar(game, input);
    DrawHud(game, input);
    DrawLevelBanner(game);
    DrawSpawnGauge(game);
    DrawReticle(input);
}

void Render_Present(void)
{
    GRRLIB_Render();
}
