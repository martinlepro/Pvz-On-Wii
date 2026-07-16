/**
 * game.cpp - Grid logic, tile-cursor planting, falling sun, and lightweight
 * passive simulation.
 */
#include "game.h"

#include <ogc/lwp_watchdog.h>
#include <stdlib.h>
#include <string.h>

/* Seed bar slot -> plant type mapping */
static const PlantType s_seedBarPlants[SEED_SLOT_COUNT] = {
    PLANT_PEASHOOTER,
    PLANT_SUNFLOWER,
    PLANT_WALLNUT,
    PLANT_CHERRY_BOMB,
    PLANT_REPEATER,
    PLANT_SNOW_PEA
};

static const u16 s_plantCosts[PLANT_TYPE_COUNT] = {
    COST_PEASHOOTER,
    COST_SUNFLOWER,
    COST_WALLNUT,
    COST_CHERRY_BOMB,
    COST_REPEATER,
    COST_SNOW_PEA
};

static const u8 s_plantMaxHealth[PLANT_TYPE_COUNT] = {
    100, /* Peashooter */
    80,  /* Sunflower */
    250, /* Wall-nut (capped at u8 max; real games use a bigger HP field) */
    50,  /* Cherry Bomb (instant, low HP placeholder) */
    100, /* Repeater */
    100  /* Snow Pea */
};

static const char* s_plantNames[PLANT_TYPE_COUNT] = {
    "Peashooter",
    "Sunflower",
    "Wall-nut",
    "Cherry Bomb",
    "Repeater",
    "Snow Pea"
};

/* -------------------------------------------------------------------------- */
/* Internal helpers                                                           */
/* -------------------------------------------------------------------------- */

static PlantType SlotToPlant(s8 slot)
{
    if (slot < 0 || slot >= SEED_SLOT_COUNT)
        return PLANT_NONE;
    return s_seedBarPlants[slot];
}

static bool CanAfford(const GameContext* game, PlantType type)
{
    if (type < 0 || type >= PLANT_TYPE_COUNT)
        return false;
    return game->sun >= s_plantCosts[type];
}

static bool TryPlantAtCell(GameContext* game, s8 col, s8 row, PlantType type)
{
    if (!Game_IsValidCell(col, row))
        return false;
    if (type < 0 || type >= PLANT_TYPE_COUNT)
        return false;
    if (game->grid[row][col].plant != PLANT_NONE)
        return false;
    if (!CanAfford(game, type))
        return false;

    game->grid[row][col].plant = type;
    game->grid[row][col].health = s_plantMaxHealth[type];
    game->grid[row][col].shootTimer = 0;
    game->sun -= s_plantCosts[type];
    return true;
}

static s8 ClampS8(s16 v, s16 lo, s16 hi)
{
    if (v < lo) v = lo;
    if (v > hi) v = hi;
    return (s8)v;
}

static void SpawnSunDrop(GameContext* game)
{
    for (u8 i = 0; i < MAX_SUN_DROPS; ++i)
    {
        SunDrop* s = &game->suns[i];
        if (s->active)
            continue;

        s16 lawnW = game->cellW * GRID_COLS;
        s16 margin = 20;
        s16 span = (lawnW > margin * 2) ? (lawnW - margin * 2) : lawnW;

        s->active    = true;
        s->x         = (f32)(game->lawnX + margin + (span > 0 ? (rand() % span) : 0));
        s->y         = (f32)(game->lawnY - 10);
        u8 targetRow = (u8)(rand() % GRID_ROWS);
        s->targetY   = (f32)(game->lawnY + targetRow * game->cellH + game->cellH / 2);
        s->restFrames = 0;
        return;
    }
    /* All slots occupied -- skip this spawn, timer still resets below. */
}

static void UpdateSunDrops(GameContext* game, const InputState* input)
{
    if (game->sunSpawnTimer > 0)
        game->sunSpawnTimer--;

    if (game->sunSpawnTimer == 0)
    {
        SpawnSunDrop(game);
        game->sunSpawnTimer = SUN_SPAWN_INTERVAL_FRAMES;
    }

    for (u8 i = 0; i < MAX_SUN_DROPS; ++i)
    {
        SunDrop* s = &game->suns[i];
        if (!s->active)
            continue;

        if (s->y < s->targetY)
        {
            s->y += SUN_FALL_SPEED;
            if (s->y > s->targetY)
                s->y = s->targetY;
        }
        else
        {
            s->restFrames++;
            if (s->restFrames > SUN_LIFETIME_FRAMES)
            {
                s->active = false;
                continue;
            }
        }

        /* Collected by pointing the Wiimote IR cursor at it -- no button
         * press required, matching a light-gun-style "hover to select". */
        if (input && input->cursorValid)
        {
            f32 dx = input->cursor.x - s->x;
            f32 dy = input->cursor.y - s->y;
            if ((dx * dx + dy * dy) <= (SUN_COLLECT_RADIUS * SUN_COLLECT_RADIUS))
            {
                u32 total = (u32)game->sun + SUN_VALUE;
                game->sun = (u16)(total > 9999 ? 9999 : total);
                s->active = false;
            }
        }
    }
}

/* -------------------------------------------------------------------------- */
/* Public API                                                                 */
/* -------------------------------------------------------------------------- */

void Game_Init(GameContext* game)
{
    if (!game)
        return;

    memset(game, 0, sizeof(GameContext));

    game->lawnX = LAWN_MARGIN_X;
    game->lawnY = LAWN_MARGIN_Y;
    game->cellW = CELL_WIDTH_DEFAULT;
    game->cellH = CELL_HEIGHT_DEFAULT;

    srand(gettick());

    Game_Reset(game);
}

void Game_Reset(GameContext* game)
{
    if (!game)
        return;

    for (s8 row = 0; row < GRID_ROWS; ++row)
    {
        for (s8 col = 0; col < GRID_COLS; ++col)
        {
            game->grid[row][col].plant = PLANT_NONE;
            game->grid[row][col].health = 0;
            game->grid[row][col].shootTimer = 0;
        }
    }

    game->state = GAME_STATE_PLAYING;
    game->selectedSlot = 0;
    game->heldPlant = PLANT_NONE;
    game->isHoldingSeed = false;
    game->plantCol = GRID_COLS / 2;
    game->plantRow = GRID_ROWS / 2;
    game->sun = STARTING_SUN;
    game->frameCount = 0;

    memset(game->suns, 0, sizeof(game->suns));
    game->sunSpawnTimer = SUN_SPAWN_INTERVAL_FRAMES / 2; /* first sun arrives a bit sooner */
}

bool Game_IsValidCell(s8 col, s8 row)
{
    return col >= 0 && col < GRID_COLS && row >= 0 && row < GRID_ROWS;
}

void Game_GridToScreen(const GameContext* game, s8 col, s8 row, s16* outX, s16* outY)
{
    if (!game || !outX || !outY)
        return;
    *outX = game->lawnX + (s16)col * game->cellW;
    *outY = game->lawnY + (s16)row * game->cellH;
}

u16 Game_PlantCost(PlantType type)
{
    if (type < 0 || type >= PLANT_TYPE_COUNT)
        return 0xFFFF;
    return s_plantCosts[type];
}

const char* Game_PlantName(PlantType type)
{
    if (type < 0 || type >= PLANT_TYPE_COUNT)
        return "None";
    return s_plantNames[type];
}

void Game_CancelHold(GameContext* game)
{
    if (!game)
        return;
    game->isHoldingSeed = false;
    game->heldPlant = PLANT_NONE;
}

void Game_HandleInput(GameContext* game, const InputState* input)
{
    if (!game || !input || game->state != GAME_STATE_PLAYING)
        return;

    /* Hovering a seed-bar slot with the IR cursor updates the selection;
     * it does not by itself grab the seed (A/B does that below). */
    if (input->hoveredSeedSlot >= 0)
        game->selectedSlot = input->hoveredSeedSlot;

    /* Tile-cursor movement: Button 1/2 (left-right) plus, if a Nunchuk is
     * connected, one discrete tile per stick flick in any direction. Both
     * sources are available at the same time and simply add together. */
    s16 col = game->plantCol;
    s16 row = game->plantRow;

    if (input->moveRightPressed) col += 1;
    if (input->moveLeftPressed)  col -= 1;
    col += input->tileMoveX;
    row += input->tileMoveY;

    game->plantCol = ClampS8(col, PLANT_CURSOR_COL_MIN, PLANT_CURSOR_COL_MAX);
    game->plantRow = ClampS8(row, PLANT_CURSOR_ROW_MIN, PLANT_CURSOR_ROW_MAX);

    /* Press A/B: grab the currently selected seed (shown semi-transparent). */
    if (input->placePressed)
    {
        PlantType selected = SlotToPlant(game->selectedSlot);
        if (selected != PLANT_NONE && CanAfford(game, selected))
        {
            game->isHoldingSeed = true;
            game->heldPlant = selected;
        }
    }

    /* Release A/B: plant at the tile-cursor, unless it has been pushed
     * off the lawn ("far beyond the grass tiles"), in which case the hold
     * is simply cancelled with no cost. */
    if (input->placeReleased)
    {
        if (game->isHoldingSeed)
        {
            if (Game_IsValidCell(game->plantCol, game->plantRow))
                TryPlantAtCell(game, game->plantCol, game->plantRow, game->heldPlant);
            Game_CancelHold(game);
        }
    }
}

void Game_Update(GameContext* game, const InputState* input)
{
    if (!game || game->state != GAME_STATE_PLAYING)
        return;

    game->frameCount++;

    /*
     * Placeholder passive sun from sunflowers (every ~5 s at 60 FPS).
     * Extend with zombie waves, projectiles, and collision in future modules.
     */
    if ((game->frameCount % 300) == 0)
    {
        for (s8 row = 0; row < GRID_ROWS; ++row)
        {
            for (s8 col = 0; col < GRID_COLS; ++col)
            {
                if (game->grid[row][col].plant == PLANT_SUNFLOWER)
                {
                    u32 total = (u32)game->sun + 25;
                    game->sun = (u16)(total > 9999 ? 9999 : total);
                }
            }
        }
    }

    UpdateSunDrops(game, input);
}
