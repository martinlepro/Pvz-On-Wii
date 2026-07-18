/**
 * game.cpp - Grid logic, tile-cursor planting, falling sun, and lightweight
 * passive simulation.
 */
#include "game.h"
#include "zombie.h"
#include "combat.h"
#include "level.h"
#include "minigame.h"
#include "particle.h"
#include "survival.h"

#include <ogc/lwp_watchdog.h>
#include <stdlib.h>
#include <string.h>
#include <math.h>

/* -------------------------------------------------------------------------- */
/* Internal helpers                                                           */
/* -------------------------------------------------------------------------- */

static PlantType SlotToPlant(const GameContext* game, s8 slot)
{
    if (slot < 0 || slot >= (s8)game->selectedCount)
        return PLANT_NONE;
    return game->selectedPlants[slot];
}

static bool CanAfford(const GameContext* game, PlantType type)
{
    if (type < 0 || type >= PLANT_TYPE_COUNT)
        return false;
    u16 extra = 0;
    if (Survival_IsSurvival(game->currentLevelIndex))
        extra = Survival_ExtraCost(game, type);
    else
        extra = Minigame_ExtraCost(game, type);
    u16 total = Plant_Cost(type) + extra;
    return game->sun >= total;
}

static bool TryPlantAtCell(GameContext* game, s8 col, s8 row, PlantType type)
{
    if (!Game_IsValidCellForWorld(game, col, row))
        return false;
    if (type < 0 || type >= PLANT_TYPE_COUNT)
        return false;
    GridCell* cell = &game->grid[row][col];
    if (!CanAfford(game, type))
        return false;

    u32 wf = game->worldFlags;
    bool isWater = (wf & WORLD_FLAG_HAS_WATER) && (row == 2 || row == 3);

    /* Pumpkin can be placed over an existing plant */
    if (type == PLANT_PUMPKIN)
    {
        if (cell->plant == PLANT_NONE)
        {
            u16 extra = 0;
            if (Survival_IsSurvival(game->currentLevelIndex)) extra = Survival_ExtraCost(game, type);
            else extra = Minigame_ExtraCost(game, type);
            game->sun -= Plant_Cost(type) + extra;
            cell->plant = PLANT_PUMPKIN;
            cell->health = Plant_MaxHealth(type);
            cell->shootTimer = 0;
            return true;
        }
        if (cell->plant != PLANT_PUMPKIN && !cell->hasPumpkin)
        {
            u16 extra = 0;
            if (Survival_IsSurvival(game->currentLevelIndex)) extra = Survival_ExtraCost(game, type);
            else extra = Minigame_ExtraCost(game, type);
            game->sun -= Plant_Cost(type) + extra;
            cell->hasPumpkin = true;
            cell->pumpkinHealth = Plant_MaxHealth(type);
            return true;
        }
        return false;
    }

    /* Lily Pad / Flower Pot special placement */
    if (type == PLANT_LILY_PAD && isWater)
    {
        cell->hasLilyPad = true;
    }

    if (type == PLANT_FLOWER_POT && (wf & WORLD_FLAG_NEEDS_POTS))
    {
        cell->hasFlowerPot = true;
    }

    /* Normal plant: cell must be empty */
    if (cell->plant != PLANT_NONE)
        return false;

    /* World-specific planting rules */
    if (isWater && type != PLANT_LILY_PAD && type != PLANT_TANGLE_KELP && type != PLANT_SEA_SHROOM)
        return false;
    if (wf & WORLD_FLAG_NEEDS_POTS)
    {
        if (type != PLANT_FLOWER_POT && !cell->hasFlowerPot)
            return false;
    }

    u16 extra = 0;
    if (Survival_IsSurvival(game->currentLevelIndex)) extra = Survival_ExtraCost(game, type);
    else extra = Minigame_ExtraCost(game, type);
    game->sun -= Plant_Cost(type) + extra;
    cell->plant = type;
    cell->health = Plant_MaxHealth(type);
    cell->shootTimer = 0;

    if (type == PLANT_CHERRY_BOMB) {
        Combat_TriggerCherryBomb(game, col, row);
    }
    Minigame_OnPlantPlaced(game, col, row, type);
    return true;
}

static s8 ClampS8(s16 v, s16 lo, s16 hi)
{
    if (v < lo) v = lo;
    if (v > hi) v = hi;
    return (s8)v;
}

/** Nearest grid cell to a screen position, clamped inside the lawn. */
static void ScreenToNearestCell(const GameContext* game, f32 x, f32 y, s8* outCol, s8* outRow)
{
    f32 relX = (x - (f32)game->lawnX) / (f32)game->cellW;
    f32 relY = (y - (f32)game->lawnY) / (f32)game->cellH;
    *outCol = ClampS8((s16)floorf(relX), 0, GRID_COLS - 1);
    *outRow = ClampS8((s16)floorf(relY), 0, (s16)(game->rowCount - 1));
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
        s->fromSunflower = false;
        s->phase     = SUN_PHASE_FALLING;
        s->x         = (f32)(game->lawnX + margin + (span > 0 ? (rand() % span) : 0));
        s->y         = (f32)(-SUN_ICON_SIZE * 2);
        u8 targetRow = (u8)(rand() % game->rowCount);
        s->targetY   = (f32)(game->lawnY + targetRow * game->cellH + game->cellH / 2);
        s->peakY     = s->targetY;
        s->restFrames = 0;
        return;
    }
}

static bool SpawnSunDropAt(GameContext* game, f32 x, f32 targetY, bool fromSunflower)
{
    for (u8 i = 0; i < MAX_SUN_DROPS; ++i)
    {
        SunDrop* s = &game->suns[i];
        if (s->active)
            continue;

        s->active    = true;
        s->fromSunflower = fromSunflower;
        s->phase     = SUN_PHASE_RISING;
        s->x         = x;
        s->y         = targetY - 20.0f;
        s->targetY   = targetY;
        s->peakY     = targetY - 35.0f;
        s->restFrames = 0;
        return true;
    }
    return false;
}

static void UpdateSunDrops(GameContext* game, const InputState* input)
{
    for (u8 i = 0; i < MAX_SUN_DROPS; ++i)
    {
        SunDrop* s = &game->suns[i];
        if (!s->active)
            continue;

        switch (s->phase)
        {
            case SUN_PHASE_RISING:
            {
                s->y -= 2.0f;
                if (s->y <= s->peakY)
                    s->phase = SUN_PHASE_DROPPING;
                break;
            }

            case SUN_PHASE_DROPPING:
            {
                s->y += 1.5f;
                if (s->y >= s->targetY + 8.0f)
                    s->phase = SUN_PHASE_RESTING;
                break;
            }

            case SUN_PHASE_FALLING:
            {
                if (s->y < s->targetY)
                {
                    s->y += SUN_FALL_SPEED;
                    if (s->y > s->targetY)
                        s->y = s->targetY;
                }
                if (s->y >= s->targetY)
                    s->phase = SUN_PHASE_RESTING;
                break;
            }

            case SUN_PHASE_RESTING:
            {
                s->restFrames++;
                if (s->restFrames > SUN_LIFETIME_FRAMES)
                {
                    s->active = false;
                    continue;
                }
                break;
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
                f32 tx = (s->x - (f32)game->lawnX) / (f32)game->cellW;
                f32 ty = (s->y - (f32)game->lawnY) / (f32)game->cellH;
                Particle_Spawn(game->particles, PARTICLE_SUN_SPARKLE, tx, ty, 5);
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

    /* Title screen shows first; user picks Adventure to start.
     * Just init default geometry. */
    game->rowCount = 5;
    game->state = GAME_STATE_PLAYING;
    game->sun = STARTING_SUN;
    game->survivalFlags = 0;
    game->wavesSinceReselect = 0;
    game->survivalWaveSet = 0;
    game->isSurvivalReselect = false;
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
            game->grid[row][col].hasLilyPad = false;
            game->grid[row][col].hasFlowerPot = false;
            game->grid[row][col].hasGrave = false;
            game->grid[row][col].hasPumpkin = false;
            game->grid[row][col].pumpkinHealth = 0;
            game->grid[row][col].hasLadder = false;
        }
    }

    game->state = GAME_STATE_PLAYING;
    game->selectedSlot = 0;
    game->heldPlant = PLANT_NONE;
    game->isHoldingSeed = false;
    game->plantCol = GRID_COLS / 2;
    game->plantRow = (s8)(game->rowCount / 2);
    game->sun = Minigame_StartingSun(game);
    game->frameCount = 0;
    game->isSurvivalReselect = false;

    memset(game->suns, 0, sizeof(game->suns));
    game->sunSpawnTimer = SUN_SPAWN_INTERVAL_FRAMES / 2;
    memset(game->particles, 0, sizeof(game->particles));
}

bool Game_IsValidCell(s8 col, s8 row)
{
    return col >= 0 && col < GRID_COLS && row >= 0 && row < GRID_ROWS;
}

bool Game_IsValidCellForWorld(const GameContext* game, s8 col, s8 row)
{
    return col >= 0 && col < GRID_COLS && row >= 0 && row < (s8)game->rowCount;
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
    return Plant_Cost(type);
}

const char* Game_PlantName(PlantType type)
{
    return Plant_Name(type);
}

void Game_CancelHold(GameContext* game)
{
    if (!game)
        return;
    game->isHoldingSeed = false;
    game->heldPlant = PLANT_NONE;
}

/* -------------------------------------------------------------------------- */
/* Plant selection screen helpers                                            */
/* -------------------------------------------------------------------------- */

void Game_ToggleSelection(GameContext* game, PlantType type)
{
    if (!game || type < 0 || type >= PLANT_TYPE_COUNT)
        return;

    /* Already selected? Remove it. */
    for (u8 i = 0; i < game->selectedCount; ++i)
    {
        if (game->selectedPlants[i] == type)
        {
            /* Shift remaining slots left */
            for (u8 j = i; j + 1 < game->selectedCount; ++j)
                game->selectedPlants[j] = game->selectedPlants[j + 1];
            game->selectedCount--;
            return;
        }
    }

    /* Not selected yet -- add if there's room */
    if (game->selectedCount < MAX_SELECTED_PLANTS)
    {
        game->selectedPlants[game->selectedCount++] = type;
    }
}

void Game_ConfirmSelection(GameContext* game)
{
    if (!game)
        return;

    if (game->isSurvivalReselect)
    {
        game->isSurvivalReselect = false;
        Zombie_ResetAll(game->zombies);
        Combat_ResetAll(game);
        Survival_GenerateNextWaves(game);
        game->state = GAME_STATE_PLAYING;
    }
    else
    {
        if (game->selectedCount == 0)
            return;
        Level_StartPlaying(game);
    }
}

void Game_HandleInput(GameContext* game, const InputState* input)
{
    if (!game || !input)
        return;

    if (game->state == GAME_STATE_SELECTING)
    {
        /* Navigate the selection grid with D-pad / stick */
        s16 cursor = game->selectionCursor;
        cursor += input->tileMoveX + input->tileMoveY * 5; /* ~5 plants per row */
        if (cursor < 0) cursor = 0;
        if (cursor >= (s16)game->unlockedCount) cursor = (s16)(game->unlockedCount - 1);
        game->selectionCursor = (s8)cursor;

        /* Toggle the currently hovered plant */
        if (input->placePressed && game->unlockedCount > 0)
        {
            PlantType type = game->unlockedPlants[game->selectionCursor];
            Game_ToggleSelection(game, type);
        }

        /* Confirm selection and start level (using menu toggle button,
         * which is blocked from opening the actual menu during SELECTING) */
        if (input->menuTogglePressed && game->selectedCount > 0)
        {
            Game_ConfirmSelection(game);
        }
        return;
    }

    if (game->state != GAME_STATE_PLAYING)
        return;

    /* Hovering a seed-bar slot with the IR cursor updates the selection
     * (used for the highlight, and as the fallback-path grab target). */
    if (input->hoveredSeedSlot >= 0)
        game->selectedSlot = input->hoveredSeedSlot;

    if (input->cursorValid)
    {
        /*
         * Primary control scheme: point-and-click, following the original
         * game's seed-packet feel.
         *   - Click a seed at the top      -> pick it up; it follows the cursor.
         *   - Click the top again while held -> cancel, no cost.
         *   - Click anywhere else while held -> plant on the nearest square
         *     (a miss, e.g. an occupied tile, just keeps it held so the
         *     player can try another spot -- it doesn't cancel).
         * The tile-cursor (used for the placement-preview highlight) tracks
         * the nearest square to the IR cursor every frame, not just on click.
         */
        ScreenToNearestCell(game, input->cursor.x, input->cursor.y, &game->plantCol, &game->plantRow);

        if (input->placePressed)
        {
            if (!game->isHoldingSeed)
            {
                if (input->hoveredSeedSlot >= 0)
                {
                    PlantType selected = SlotToPlant(game, game->selectedSlot);
                    if (selected != PLANT_NONE && CanAfford(game, selected))
                    {
                        game->isHoldingSeed = true;
                        game->heldPlant = selected;
                    }
                }
            }
            else if (input->hoveredSeedSlot >= 0)
            {
                Game_CancelHold(game);
            }
            else
            {
                if (TryPlantAtCell(game, game->plantCol, game->plantRow, game->heldPlant))
                    Game_CancelHold(game);
            }
        }
    }
    else
    {
        /* Fallback control scheme for when the sensor bar is out of range
         * and there's no IR cursor to click with:
         *   - D-pad and/or Nunchuk stick: move the discrete tile-cursor.
         *   - Button 1/2: cycle which seed is selected.
         *   - A/B: plant the selected seed at the cursor, one press, no
         *     separate pick-up/drag step (there's no cursor to drag with).
         */
        s16 col = game->plantCol;
        s16 row = game->plantRow;
        col += input->tileMoveX;
        row += input->tileMoveY;
        s8 rowMax = (s8)(game->rowCount - 1 + PLANT_CURSOR_OVERSHOOT);
        game->plantCol = ClampS8(col, PLANT_CURSOR_COL_MIN, PLANT_CURSOR_COL_MAX);
        game->plantRow = ClampS8(row, PLANT_CURSOR_ROW_MIN, rowMax);

        if (game->selectedCount > 0)
        {
            if (input->moveRightPressed)
                game->selectedSlot = (s8)((game->selectedSlot + 1) % game->selectedCount);
            if (input->moveLeftPressed)
                game->selectedSlot = (s8)((game->selectedSlot + game->selectedCount - 1) % game->selectedCount);
        }

        if (input->placePressed && Game_IsValidCellForWorld(game, game->plantCol, game->plantRow))
        {
            PlantType selected = SlotToPlant(game, game->selectedSlot);
            if (selected != PLANT_NONE)
                TryPlantAtCell(game, game->plantCol, game->plantRow, selected);
        }
    }
}

void Game_Update(GameContext* game, const InputState* input)
{
    if (!game)
        return;

    if (game->state == GAME_STATE_GAME_OVER)
        return;

    if (game->state != GAME_STATE_PLAYING && game->state != GAME_STATE_LEVEL_WON)
        return;

    game->frameCount++;

    if (game->state == GAME_STATE_PLAYING)
    {
        /* Passive sun from sunflowers */
        if ((game->frameCount % 300) == 0)
        {
            for (s8 row = 0; row < (s8)game->rowCount; ++row)
            {
                for (s8 col = 0; col < GRID_COLS; ++col)
                {
                    if (game->grid[row][col].plant == PLANT_SUNFLOWER || game->grid[row][col].plant == PLANT_TWIN_SUNFLOWER)
                    {
                        f32 cellCX = (f32)(game->lawnX + col * game->cellW + game->cellW / 2);
                        f32 cellCY = (f32)(game->lawnY + row * game->cellH);
                        SpawnSunDropAt(game, cellCX, cellCY, true);
                    }
                }
            }
        }

        /* Spawn sky suns only if the world/minigame allows it; always animate
         * existing suns (the loop inside UpdateSunDrops handles both). */
        if ((game->worldFlags & WORLD_FLAG_SUN_FALLS) && Minigame_SunFalls(game))
        {
            if (game->sunSpawnTimer > 0)
                game->sunSpawnTimer--;
            if (game->sunSpawnTimer == 0)
            {
                SpawnSunDrop(game);
                game->sunSpawnTimer = SUN_SPAWN_INTERVAL_FRAMES;
            }
        }
        UpdateSunDrops(game, input);
        Combat_Update(game);
        Particle_Update(game->particles);

        if (Zombie_UpdateAll(game))
        {
            game->state = GAME_STATE_GAME_OVER;
            game->levelIntermissionTimer = TARGET_FPS * 2;
            return;
        }
    }

    /* Handles wave spawning while playing, and the win-intermission ->
     * auto-advance-to-next-level countdown while GAME_STATE_LEVEL_WON. */
    Level_Update(game);

    /* Survival: after a flag is cleared, transition to reselection screen */
    if (game->state == GAME_STATE_PLAYING && game->isSurvivalReselect)
    {
        Plant_GetUnlockedForLevel(game->currentLevelIndex, game->unlockedPlants, &game->unlockedCount);
        game->selectedCount = 0;
        game->selectionCursor = 0;
        game->state = GAME_STATE_SELECTING;
    }
}
