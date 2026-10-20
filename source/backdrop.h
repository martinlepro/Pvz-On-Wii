/**
 * backdrop.h - Lawn "camera": places the level background art on screen
 * WITHOUT stretching it.
 *
 * The background PNGs (the PNGs in assets/background/) are ~800x340 pictures of the
 * whole yard, while the screen is 640x480, so drawing them into a
 * SCREEN_WIDTH x SCREEN_HEIGHT quad (what the game used to do) squashed them
 * horizontally and stretched them vertically. Instead:
 *
 *   * One uniform scale factor is used for both axes. It is chosen so that
 *     one lane of the painted lawn is exactly CELL_HEIGHT_DEFAULT pixels
 *     tall -- the lawn therefore always has the same on-screen height
 *     (5 lanes = 5 * CELL_HEIGHT_DEFAULT) whatever the world or the screen
 *     size, which is the invariant everything else is built on.
 *   * The grid's column width (cellW) is taken from the art too (the painted
 *     cells are narrower than they are tall), so plants and zombies still
 *     line up with the checkerboard.
 *   * The camera is centred on the lawn: the lawn is centred horizontally on
 *     the screen, and what is left on each side is simply "more picture"
 *     (house, fence, pavement and the edge of the road). On a wider screen
 *     more of the yard shows; the lawn itself never changes size.
 *   * Wherever the picture is smaller than the screen it is extended instead of
 *     being stretched to fit: a few pixels above/below the picture continue its
 *     first/last rows, and far to the sides (very wide screens only) the picture
 *     is mirror-tiled.
 */
#ifndef PVZ_BACKDROP_H
#define PVZ_BACKDROP_H

#include <gctypes.h>
#include <grrlib.h>
#include "constants.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef enum {
    BD_DAY = 0,   /* every lawn day / no lawn day / (shares the day geometry) */
    BD_NIGHT,
    BD_POOL,      /* 6 lanes, lanes 3-4 are the pool                         */
    BD_FOG,       /* 6 lanes, same layout as the pool                         */
    BD_ROOF,      /* roof + the Dr. Zomboss final-fight roof                  */
    BD_COUNT
} BackdropKind;

typedef struct {
    BackdropKind kind;
    u8   fullRows;     /* lanes painted in the art: 5 or 6                        */
    s16  lawnX;        /* screen x of the left edge of column 0                    */
    s16  lawnTop;      /* screen y of the top edge of the FULL (fullRows) lawn     */
    s16  cellW;        /* grid cell size on screen                                 */
    s16  cellH;        /* always CELL_HEIGHT_DEFAULT                               */
    f32  scale;        /* screen pixels per art pixel (same on both axes)          */
    f32  originX;      /* screen position of the art's (cropped) top-left corner   */
    f32  originY;
    f32  cropX, cropY, cropW, cropH;  /* usable part of the texture, texture px    */
} BackdropLayout;

/** Layout for a background kind. `tex` is only used to learn the texture size
 *  (may be NULL -> the nominal 800x340 is assumed). */
BackdropLayout Backdrop_Make(BackdropKind kind, const GRRLIB_texImg* tex);

/** World index (World enum value, 0..4) -> BackdropKind. */
BackdropKind Backdrop_KindForWorld(int world);

/** Screen y of the top of a lawn that has only `rows` lanes (<= fullRows):
 *  the playable lanes sit in the middle of the painted lawn. */
s16 Backdrop_LawnY(const BackdropLayout* layout, u8 rows);

/** Draw `tex` with the camera layout, restricted to the screen rectangle
 *  (clipX, clipY, clipW, clipH). Use (0, 0, SCREEN_WIDTH, SCREEN_HEIGHT)
 *  for the whole screen. Leaves the GRRLIB clip rect reset to full screen. */
void Backdrop_Draw(GRRLIB_texImg* tex, const BackdropLayout* layout,
                   int clipX, int clipY, int clipW, int clipH);

/** Screen rectangle of the painted pool water (BD_POOL / BD_FOG only);
 *  returns false for the other kinds. */
bool Backdrop_GetWaterRect(const BackdropLayout* layout, int* x, int* y, int* w, int* h);

#ifdef __cplusplus
}
#endif

#endif /* PVZ_BACKDROP_H */
