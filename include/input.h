/**
 * input.h - Wiimote and optional Nunchuk input handling.
 *
 * Controls (defaults; remappable in the pause menu, see settings.h):
 *   A / B (hold)   - show the selected seed semi-transparent; release plants
 *                    it at the current tile-cursor position
 *   D-pad          - move the tile-cursor one tile per press, any direction
 *   Nunchuk stick  - same, one tile per flick, any direction (independent of
 *                    the D-pad above; only used when a Nunchuk is connected)
 *   1              - select the next seed in the seed bar
 *   2              - select the previous seed in the seed bar
 *   - / +          - open/close the options menu
 *   Wiimote IR     - on-screen pointer; when active this takes over as the
 *                    primary scheme instead (click a seed to pick it up, it
 *                    follows the pointer, click again to cancel/place -- see
 *                    Game_HandleInput). D-pad/Nunchuk/1/2 above are the
 *                    fallback used while the Wiimote isn't pointed at the
 *                    sensor bar.
 *   HOME           - exit (never remappable)
 */
#ifndef PVZ_INPUT_H
#define PVZ_INPUT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "settings.h"
#include "types.h"
#include <gctypes.h>
#include <stdbool.h>

typedef struct {
    Vec2  cursor;              /* IR pointer position (screen space) */
    bool  cursorValid;         /* true while the Wiimote is pointed at the sensor bar */

    s8    hoveredSeedSlot;     /* seed-bar slot under the IR cursor this frame, -1 if none */

    bool  nunchukConnected;

    bool  placeHeld;           /* Place action (A/B by default) currently held */
    bool  placePressed;        /* Place action pressed this frame (edge) */
    bool  placeReleased;       /* Place action released this frame (edge) */

    bool  moveRightPressed;    /* Button 1 (or remapped): select next seed */
    bool  moveLeftPressed;     /* Button 2 (or remapped): select previous seed */

    s8    tileMoveX;           /* -1/0/+1: discrete tile-cursor move this frame (D-pad + Nunchuk) */
    s8    tileMoveY;           /* -1/0/+1: discrete tile-cursor move this frame (D-pad + Nunchuk) */

    bool  menuTogglePressed;   /* - or + (or remapped) pressed this frame */
} InputState;

/** Zero state and configure the Wiimote data format once. */
void Input_Init(InputState* input);

/** Poll controllers and update cursor / logical action flags for one frame.
 *  Bindings are read from `settings` every call so a remap takes effect
 *  immediately. */
void Input_Update(InputState* input, const Settings* settings);

#ifdef __cplusplus
}
#endif

#endif /* PVZ_INPUT_H */
