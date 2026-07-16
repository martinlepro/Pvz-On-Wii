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

#include <grrlib.h>
#include <stdio.h>
#include <string.h>

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

static PlantType SlotToPlantDisplay(s8 slot)
{
    switch (slot)
    {
        case 0: return PLANT_PEASHOOTER;
        case 1: return PLANT_SUNFLOWER;
        case 2: return PLANT_WALLNUT;
        case 3: return PLANT_CHERRY_BOMB;
        case 4: return PLANT_REPEATER;
        case 5: return PLANT_SNOW_PEA;
        default: return PLANT_NONE;
    }
}

/* -------------------------------------------------------------------------- */
/* Optional runtime-loaded assets (see ASSETS.md for exact paths/sizes)      */
/* -------------------------------------------------------------------------- */

typedef struct {
    GRRLIB_texImg* seedIcon[PLANT_TYPE_COUNT];
    GRRLIB_texImg* plantSprite[PLANT_TYPE_COUNT];
    GRRLIB_texImg* sunIcon;
    GRRLIB_texImg* cursorIcon;
} RenderAssets;

static RenderAssets     s_assets;
static GRRLIB_ttfFont*  s_font = NULL;

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

static void DrawLabel(s16 x, s16 y, u32 color, unsigned int size, const char* text)
{
    if (s_font && text)
        GRRLIB_PrintfTTF(x, y, s_font, text, size, color);
}

/** Draws `tex` scaled to fit (w,h) at (x,y) if loaded; otherwise a flat rect. */
static void DrawTexturedBox(GRRLIB_texImg* tex, s16 x, s16 y, s16 w, s16 h, u32 tint, u32 fallbackColor)
{
    if (tex && tex->data)
    {
        f32 scaleX = (tex->w > 0) ? ((f32)w / (f32)tex->w) : 1.0f;
        f32 scaleY = (tex->h > 0) ? ((f32)h / (f32)tex->h) : 1.0f;
        GRRLIB_DrawImg((f32)x, (f32)y, tex, 0.0f, scaleX, scaleY, tint);
    }
    else
    {
        DrawRect(x, y, w, h, fallbackColor);
    }
}

/* -------------------------------------------------------------------------- */
/* Frame sections                                                             */
/* -------------------------------------------------------------------------- */

static void DrawSeedBar(const GameContext* game, const InputState* input)
{
    DrawRect(0, 0, SCREEN_WIDTH, SEED_BAR_HEIGHT, COL_SEED_BAR);

    s16 slotY = (SEED_BAR_HEIGHT - SEED_SLOT_HEIGHT) / 2;

    for (s8 i = 0; i < SEED_SLOT_COUNT; ++i)
    {
        s16 slotX = SEED_BAR_PADDING + i * (SEED_SLOT_WIDTH + SEED_BAR_PADDING);

        bool isSelected = (i == game->selectedSlot);
        bool isHovered  = (i == input->hoveredSeedSlot);
        u32  slotColor  = isSelected ? COL_SLOT_SEL : (isHovered ? COL_SLOT_HOVER : COL_SLOT);

        DrawRect(slotX, slotY, SEED_SLOT_WIDTH, SEED_SLOT_HEIGHT, 0x1A1A1AFF);
        DrawRectOutline(slotX, slotY, SEED_SLOT_WIDTH, SEED_SLOT_HEIGHT, slotColor, isSelected ? 3 : 1);

        PlantType type = SlotToPlantDisplay(i);
        const s16 iconSize = 48;
        s16 iconX = slotX + (SEED_SLOT_WIDTH - iconSize) / 2;
        s16 iconY = slotY + 3;

        DrawTexturedBox(s_assets.seedIcon[type], iconX, iconY, iconSize, iconSize, 0xFFFFFFFF, PlantColor(type));

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

    /* Sun counter, right-aligned in the seed bar */
    s16 sunX = SCREEN_WIDTH - 110;
    s16 sunY = (SEED_BAR_HEIGHT - 40) / 2;
    DrawTexturedBox(s_assets.sunIcon, sunX, sunY, 40, 40, 0xFFFFFFFF, COL_SUN_TEXT);
    if (s_font)
    {
        char sunBuf[8];
        snprintf(sunBuf, sizeof(sunBuf), "%u", (unsigned)game->sun);
        DrawLabel(sunX + 44, sunY + 12, COL_HUD_TEXT, 16, sunBuf);
    }
}

static void DrawLawnGrid(const GameContext* game)
{
    s16 lawnW = game->cellW * GRID_COLS;
    s16 lawnH = game->cellH * GRID_ROWS;
    DrawRect(game->lawnX, game->lawnY, lawnW, lawnH, COL_BG);

    for (s8 row = 0; row < GRID_ROWS; ++row)
    {
        for (s8 col = 0; col < GRID_COLS; ++col)
        {
            s16 x = game->lawnX + col * game->cellW;
            s16 y = game->lawnY + row * game->cellH;

            if ((row + col) % 2 == 0)
                DrawRect(x, y, game->cellW, game->cellH, COL_LAWN_DARK);

            const GridCell* cell = &game->grid[row][col];
            if (cell->plant != PLANT_NONE)
            {
                s16 pad = 6;
                DrawTexturedBox(s_assets.plantSprite[cell->plant],
                                 x + pad, y + pad, game->cellW - pad * 2, game->cellH - pad * 2,
                                 0xFFFFFFFF, PlantColor(cell->plant));
            }
        }
    }

    /* Faint grid lines */
    for (s8 col = 0; col <= GRID_COLS; ++col)
    {
        s16 x = game->lawnX + col * game->cellW;
        GRRLIB_Line((f32)x, (f32)game->lawnY, (f32)x, (f32)(game->lawnY + lawnH), COL_GRID_LINE);
    }
    for (s8 row = 0; row <= GRID_ROWS; ++row)
    {
        s16 y = game->lawnY + row * game->cellH;
        GRRLIB_Line((f32)game->lawnX, (f32)y, (f32)(game->lawnX + lawnW), (f32)y, COL_GRID_LINE);
    }
}

/** Outlines the tile the plant would land on if released right now, colored
 *  green when valid or red when it has been pushed off the lawn. */
static void DrawTileCursor(const GameContext* game)
{
    s16 x, y;
    Game_GridToScreen(game, game->plantCol, game->plantRow, &x, &y);
    bool valid = Game_IsValidCell(game->plantCol, game->plantRow);
    DrawRectOutline(x + 2, y + 2, game->cellW - 4, game->cellH - 4, valid ? COL_VALID : COL_INVALID, 2);
}

/** The seed currently held (A/B down) drawn semi-transparent at the
 *  tile-cursor position -- this is what "picking up" a seed looks like. */
static void DrawHeldSeed(const GameContext* game)
{
    if (!game->isHoldingSeed || game->heldPlant == PLANT_NONE)
        return;

    s16 x, y;
    Game_GridToScreen(game, game->plantCol, game->plantRow, &x, &y);
    s16 pad = 6;

    u32 tint     = 0xFFFFFF00u | (u32)HELD_SEED_ALPHA;
    u32 fallback = (PlantColor(game->heldPlant) & 0xFFFFFF00u) | (u32)HELD_SEED_ALPHA;

    DrawTexturedBox(s_assets.plantSprite[game->heldPlant],
                    x + pad, y + pad, game->cellW - pad * 2, game->cellH - pad * 2,
                    tint, fallback);
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
        DrawTexturedBox(s_assets.sunIcon, x, y, size, size, 0xFFFFFFFF, COL_SUN_TEXT);
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
        DrawTexturedBox(s_assets.cursorIcon, cx - size / 2, cy - size / 2, size, size, 0xFFFFFFFF, 0);
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

    snprintf(buf, sizeof(buf), "Seed: %s", Game_PlantName(SlotToPlantDisplay(game->selectedSlot)));
    DrawLabel(8, y, COL_HUD_TEXT, 14, buf);

    const char* mode = input->nunchukConnected ? "Nunchuk" : (input->cursorValid ? "Pointer" : "No signal");
    snprintf(buf, sizeof(buf), "Input: %s", mode);
    DrawLabel(220, y, COL_HUD_TEXT, 14, buf);
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

    /* Optional font for HUD/menu text; falls back to no text at all until
     * assets/font.ttf is added (gameplay itself never depends on it). */
    s_font = GRRLIB_LoadTTFFromFile("assets/font.ttf");

    /* Optional PNGs -- see ASSETS.md for the exact list of paths/sizes.
     * Each load gracefully returns NULL if the file isn't there yet, and
     * every draw call above falls back to a flat-colored rectangle. */
    for (int i = 0; i < PLANT_TYPE_COUNT; ++i)
    {
        s_assets.seedIcon[i]    = GRRLIB_LoadTextureFromFile(s_seedIconPaths[i]);
        s_assets.plantSprite[i] = GRRLIB_LoadTextureFromFile(s_plantSpritePaths[i]);
    }
    s_assets.sunIcon    = GRRLIB_LoadTextureFromFile("assets/ui/sun.png");
    s_assets.cursorIcon = GRRLIB_LoadTextureFromFile("assets/ui/cursor.png");

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
    memset(&s_assets, 0, sizeof(s_assets));

    if (s_font)
    {
        GRRLIB_FreeTTF(s_font);
        s_font = NULL;
    }

    GRRLIB_Exit();
}

GRRLIB_ttfFont* Render_GetFont(void)
{
    return s_font;
}

void Render_Frame(const GameContext* game, const InputState* input)
{
    GRRLIB_FillScreen(0x87CEEBFF); /* sky blue backdrop behind the lawn */

    DrawLawnGrid(game);
    DrawFallingSuns(game);
    DrawTileCursor(game);
    DrawHeldSeed(game);
    DrawSeedBar(game, input);
    DrawHud(game, input);
    DrawReticle(input);
}

void Render_Present(void)
{
    GRRLIB_Render();
}
