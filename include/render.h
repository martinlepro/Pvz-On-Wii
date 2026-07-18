/**
 * render.h - GRRLIB 2D drawing for lawn, seed bar, plants, and HUD.
 */
#ifndef PVZ_RENDER_H
#define PVZ_RENDER_H

#ifdef __cplusplus
extern "C" {
#endif

#include "game.h"
#include "input.h"

/* Forward-declared here so callers that only need the font handle (menu.cpp)
 * don't have to pull in all of <grrlib.h> through this header. */
struct GRRLIB_Font;
typedef struct GRRLIB_Font GRRLIB_ttfFont;

/** Initialize GRRLIB and load any runtime resources. */
bool Render_Init(void);

/** Free GRRLIB resources. */
void Render_Shutdown(void);

/** Clear back buffer and draw full frame. */
void Render_Frame(const GameContext* game, const InputState* input);

/** Flip/present the frame. Call this once per frame, after Render_Frame()
 *  and after any overlay (e.g. Menu_Render) so overlays composite into the
 *  same buffer instead of appearing a frame late. */
void Render_Present(void);

/** Shared TTF handle (NULL if assets/font.ttf isn't present) so other
 *  modules -- e.g. menu.cpp -- can draw text without loading their own copy. */
GRRLIB_ttfFont* Render_GetFont(void);

/** Shared bitmap font texture (NULL if TTF was loaded successfully, used as
 *  fallback when no assets/font.ttf is available). */
struct GRRLIB_texImg* Render_GetBitmapFont(void);

/** Draw text using either TTF or built-in bitmap font. */
void Render_DrawText(s16 x, s16 y, u32 color, unsigned int size, const char* text);

#ifdef __cplusplus
}
#endif

#endif /* PVZ_RENDER_H */
