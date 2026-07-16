/**
 * input.h - Wiimote and optional Nunchuk input handling.
 *
 * Controls (defaults; remappable in the pause menu, see settings.h):
 *   A / B (hold)   - show the selected seed semi-transparent; release plants
 *                    it at the current tile-cursor position
 *   1              - move the tile-cursor one tile right
 *   2              - move the tile-cursor one tile left
 *   Nunchuk stick  - move the tile-cursor one tile per flick, any direction
 *                    (independent of the buttons above; only used when a
 *                    Nunchuk is connected)
 *   - / +          - open/close the options menu
 *   Wiimote IR     - on-screen pointer; hover a seed-bar slot to select it,
 *                    or hover a falling sun to collect it
 *   HOME           - exit (never remappable)
 */
#ifndef PVZ_INPUT_H
#define PVZ_INPUT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "settings.h"
#include "types.h"
#include <ogc/types.h>
#include <stdbool.h>

typedef struct {
    Vec2  cursor;              /* IR pointer position (screen space) */
    bool  cursorValid;         /* true while the Wiimote is pointed at the sensor bar */

    s8    hoveredSeedSlot;     /* seed-bar slot under the IR cursor this frame, -1 if none */

    bool  nunchukConnected;

    bool  placeHeld;           /* Place action (A/B by default) currently held */
    bool  placePressed;        /* Place action pressed this frame (edge) */
    bool  placeReleased;       /* Place action released this frame (edge) */

    bool  moveRightPressed;    /* Button 1 (or remapped) pressed this frame */
    bool  moveLeftPressed;     /* Button 2 (or remapped) pressed this frame */

    s8    tileMoveX;           /* -1/0/+1: discrete Nunchuk tile-move this frame */
    s8    tileMoveY;           /* -1/0/+1: discrete Nunchuk tile-move this frame */

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
