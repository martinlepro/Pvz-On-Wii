/**
 * input.cpp - Wiimote + Nunchuk polling.
 *
 * Note on API usage: WPAD_IR() and WPAD_Expansion() both return void in
 * libogc: validity is reported through the struct fields they fill in
 * (ir_t::valid, expansion_t::type), not through a return code. Nunchuk stick
 * position is read via the pre-calibrated joystick_t::mag / ::ang fields
 * (magnitude 0-1, angle in degrees measured from the +Y axis) rather than
 * the raw unsigned pos.x/pos.y bytes, since those are centered around the
 * per-controller calibration value rather than zero.
 */
#include "input.h"
#include "constants.h"

#include <string.h>
#include <wiiuse/wpad.h>

/* -------------------------------------------------------------------------- */
/* Internal state (single Wiimote, channel 0)                                 */
/* -------------------------------------------------------------------------- */

static bool s_tileArmed = false; /* Nunchuk stick must return near-center before re-firing */

/* -------------------------------------------------------------------------- */
/* Internal helpers                                                           */
/* -------------------------------------------------------------------------- */

static void UpdateCursorFromIR(InputState* input)
{
    ir_t ir;
    WPAD_IR(0, &ir);

    if (ir.valid)
    {
        input->cursor.x = ir.x;
        input->cursor.y = ir.y;
        input->cursorValid = true;
    }
    else
    {
        /* Keep the last known position so the reticle doesn't jump when the
         * player briefly points off the sensor bar. */
        input->cursorValid = false;
    }
}

/** Mirrors the seed-bar slot geometry drawn in render.cpp (DrawSeedBar). */
static s8 ComputeHoveredSeedSlot(f32 x, f32 y, bool valid)
{
    if (!valid)
        return -1;

    s16 slotY = (SEED_BAR_HEIGHT - SEED_SLOT_HEIGHT) / 2;
    if (y < (f32)slotY || y >= (f32)(slotY + SEED_SLOT_HEIGHT))
        return -1;

    for (s8 i = 0; i < SEED_SLOT_COUNT; ++i)
    {
        s16 slotX = SEED_BAR_PADDING + 64 + i * (SEED_SLOT_WIDTH + SEED_BAR_PADDING);
        if (x >= (f32)slotX && x < (f32)(slotX + SEED_SLOT_WIDTH))
            return i;
    }
    return -1;
}

/** Discrete one-tile-per-flick Nunchuk movement, decoupled from the IR cursor. */
static void UpdateNunchukTileMove(InputState* input)
{
    struct expansion_t exp;
    WPAD_Expansion(0, &exp);

    if (exp.type != WPAD_EXP_NUNCHUK)
    {
        input->nunchukConnected = false;
        s_tileArmed = false;
        return;
    }

    input->nunchukConnected = true;

    f32 mag = exp.nunchuk.js.mag;
    f32 ang = exp.nunchuk.js.ang;

    if (!s_tileArmed && mag >= TILE_MOVE_ARM_MAG)
    {
        s_tileArmed = true;

        /* ang: 0 = up, 90 = right, 180 = down, 270 = left (per wiiuse docs) */
        if (ang >= 45.0f && ang < 135.0f)
            input->tileMoveX += 1;
        else if (ang >= 135.0f && ang < 225.0f)
            input->tileMoveY += 1;
        else if (ang >= 225.0f && ang < 315.0f)
            input->tileMoveX -= 1;
        else
            input->tileMoveY -= 1;
    }
    else if (mag < TILE_MOVE_REARM_MAG)
    {
        s_tileArmed = false;
    }
}

/* -------------------------------------------------------------------------- */
/* Public API                                                                 */
/* -------------------------------------------------------------------------- */

void Input_Init(InputState* input)
{
    if (!input)
        return;

    memset(input, 0, sizeof(*input));

    /* Buttons + accelerometer + IR in one packet. */
    WPAD_SetDataFormat(0, WPAD_FMT_BTNS_ACC_IR);
    WPAD_SetVRes(0, SCREEN_WIDTH, SCREEN_HEIGHT);

    input->cursor.x = SCREEN_WIDTH * 0.5f;
    input->cursor.y = SCREEN_HEIGHT * 0.5f;
    input->hoveredSeedSlot = -1;

    s_tileArmed = false;
}

void Input_Update(InputState* input, const Settings* settings)
{
    if (!input || !settings)
        return;

    /* Clear per-frame edge flags */
    input->placePressed = false;
    input->placeReleased = false;
    input->moveRightPressed = false;
    input->moveLeftPressed = false;
    input->menuTogglePressed = false;
    input->tileMoveX = 0;
    input->tileMoveY = 0;

    WPAD_ScanPads();

    u32 held = WPAD_ButtonsHeld(0);
    u32 down = WPAD_ButtonsDown(0);
    u32 up   = WPAD_ButtonsUp(0);

    u32 placeMask = settings->keyPlacePrimary | settings->keyPlaceSecondary;
    u32 menuMask  = settings->keyMenuPrimary  | settings->keyMenuSecondary;

    bool placeNow  = (held & placeMask) != 0;
    bool placeDown = (down & placeMask) != 0;
    bool placeUp   = (up   & placeMask) != 0;

    if (placeDown)
        input->placePressed = true;
    if (placeUp)
        input->placeReleased = true;
    input->placeHeld = placeNow;

    if (down & settings->keyMoveRight)
        input->moveRightPressed = true;
    if (down & settings->keyMoveLeft)
        input->moveLeftPressed = true;
    if (down & menuMask)
        input->menuTogglePressed = true;

    /* D-pad: moves the tile-cursor directly, one tile per press, in
     * whichever direction(s) are pressed this frame. The Wiimote is held
     * upright (IR must face the sensor bar for seed-bar hover/sun-collect
     * to work), so WPAD_BUTTON_UP/DOWN/LEFT/RIGHT already match the D-pad's
     * physical layout with no rotation correction needed. */
    if (down & WPAD_BUTTON_RIGHT) input->tileMoveX += 1;
    if (down & WPAD_BUTTON_LEFT)  input->tileMoveX -= 1;
    if (down & WPAD_BUTTON_DOWN)  input->tileMoveY += 1;
    if (down & WPAD_BUTTON_UP)    input->tileMoveY -= 1;

    /* IR pointer: used only for seed-bar hover selection and sun collection. */
    UpdateCursorFromIR(input);
    input->hoveredSeedSlot = ComputeHoveredSeedSlot(input->cursor.x, input->cursor.y, input->cursorValid);

    /* Nunchuk: discrete tile-cursor movement, independent of the IR pointer
     * and independent of the D-pad above -- both simply add to the same
     * tileMoveX/Y, so either (or both at once) moves the cursor. */
    UpdateNunchukTileMove(input);
}
