/**
 * backdrop.cpp - see backdrop.h.
 *
 * All the numbers in kArt[] are measured from the shipped background PNGs, in
 * TEXTURE pixels (so they stay valid whatever size the art ends up on screen).
 * If you swap in different background art, re-measure these -- nothing else
 * in the game depends on the artwork's size any more.
 *
 *   gridX, gridY   top-left corner of the painted lawn's column 0 / lane 0
 *   cellW, cellH   size of one painted lawn cell
 *   rows           painted lanes (5, or 6 for the pool/fog yards)
 *   crop*          usable part of the texture. pool.png is a 800x800 sprite
 *                  sheet whose top 22 rows are a label strip and whose lower
 *                  half holds loose pool parts; only the yard picture
 *                  (y 22..362) must ever be shown.
 *   water*         the (placeholder) pool interior, drawn over by the game
 */
#include "backdrop.h"
#include "game.h"
#include "level.h"
#include <math.h>

namespace {

struct ArtDef {
    f32 gridX, gridY, cellW, cellH;
    u8  rows;
    f32 cropY, cropH;              /* cropH <= 0 : use the whole texture height */
    f32 waterX, waterY, waterW, waterH; /* waterW <= 0 : no pool */
};

const ArtDef kArt[BD_COUNT] = {
    /* BD_DAY   */ { 140.0f, 33.0f, 48.0f, 60.0f, 5,  0.0f,   0.0f,   0, 0, 0, 0 },
    /* BD_NIGHT */ { 140.0f, 34.0f, 48.0f, 60.0f, 5,  0.0f,   0.0f,   0, 0, 0, 0 },
    /* BD_POOL  */ { 146.0f, 55.0f, 46.4f, 51.0f, 6, 22.0f, 340.0f, 145.0f, 187.0f, 403.0f, 81.0f },
    /* BD_FOG   */ { 146.0f, 34.0f, 46.4f, 51.0f, 6,  0.0f,   0.0f, 143.0f, 165.0f, 406.0f, 85.0f },
    /* BD_ROOF  */ { 135.0f, 62.0f, 54.0f, 56.0f, 5,  0.0f,   0.0f,   0, 0, 0, 0 },
};

const f32 kNominalW = 800.0f;
const f32 kNominalH = 340.0f;

inline int IMax(int a, int b) { return a > b ? a : b; }
inline int IMin(int a, int b) { return a < b ? a : b; }

} // namespace

extern "C" BackdropKind Backdrop_KindForWorld(int world)
{
    switch (world)
    {
        case WORLD_NIGHT: return BD_NIGHT;
        case WORLD_POOL:  return BD_POOL;
        case WORLD_FOG:   return BD_FOG;
        case WORLD_ROOF:  return BD_ROOF;
        default:          return BD_DAY;
    }
}

extern "C" BackdropLayout Backdrop_Make(BackdropKind kind, const GRRLIB_texImg* tex)
{
    if ((int)kind < 0 || (int)kind >= BD_COUNT) kind = BD_DAY;
    const ArtDef& a = kArt[kind];

    BackdropLayout L;
    L.kind     = kind;
    L.fullRows = a.rows;

    const f32 texW = (tex && tex->w > 0) ? (f32)tex->w : kNominalW;
    const f32 texH = (tex && tex->h > 0) ? (f32)tex->h : kNominalH;
    L.cropX = 0.0f;
    L.cropW = texW;
    L.cropY = a.cropY;
    L.cropH = (a.cropH > 0.0f) ? a.cropH : (texH - a.cropY);
    if (L.cropH > texH - L.cropY) L.cropH = texH - L.cropY;   /* texture smaller than expected */

    /* One uniform scale: a painted lane is CELL_HEIGHT_DEFAULT pixels tall. */
    L.cellH  = (s16)CELL_HEIGHT_DEFAULT;
    L.scale  = (f32)L.cellH / a.cellH;
    L.cellW  = (s16)(a.cellW * L.scale + 0.5f);

    /* Camera centred on the lawn. */
    L.lawnX   = (s16)((SCREEN_WIDTH - GRID_COLS * L.cellW) / 2);
    L.lawnTop = (s16)(LAWN_MARGIN_Y + ((GRID_ROWS - a.rows) * L.cellH) / 2);

    L.originX = (f32)L.lawnX   - (a.gridX - L.cropX) * L.scale;
    L.originY = (f32)L.lawnTop - (a.gridY - L.cropY) * L.scale;
    return L;
}

extern "C" s16 Backdrop_LawnY(const BackdropLayout* L, u8 rows)
{
    return (s16)(L->lawnTop + (((int)L->fullRows - (int)rows) * L->cellH) / 2);
}

extern "C" bool Backdrop_GetWaterRect(const BackdropLayout* L, int* x, int* y, int* w, int* h)
{
    const ArtDef& a = kArt[L->kind];
    if (a.waterW <= 0.0f) return false;
    *x = (int)(L->originX + (a.waterX - L->cropX) * L->scale + 0.5f);
    *y = (int)(L->originY + (a.waterY - L->cropY) * L->scale + 0.5f);
    *w = (int)(a.waterW * L->scale + 0.5f);
    *h = (int)(a.waterH * L->scale + 0.5f);
    return true;
}

extern "C" void Backdrop_Draw(GRRLIB_texImg* tex, const BackdropLayout* L,
                              int clipX, int clipY, int clipW, int clipH)
{
    if (!tex || !tex->data || !L || clipW <= 0 || clipH <= 0) return;

    const f32 s  = L->scale;
    const f32 tw = L->cropW * s, th = L->cropH * s;      /* the picture, on screen */
    if (tw < 1.0f || th < 1.0f) return;

    const f32 W = (f32)tex->w, H = (f32)tex->h;
    const f32 cx1 = L->cropX + L->cropW;
    const f32 picTop = L->originY, picBottom = L->originY + th;

    /* Horizontal tiles: the picture as-is, neighbours mirrored (only ever shows
     * on screens much wider than the art). */
    const int i0 = (int)floorf(((f32)clipX - L->originX) / tw);
    const int i1 = (int)floorf(((f32)(clipX + clipW - 1) - L->originX) / tw);

    /* Vertically there are three bands: the picture itself, and (if it does not
     * reach the top / bottom of the clip area) a strip above and below it that
     * continues the picture's first / last few rows stretched ("clamp to edge").
     * Mirroring was tried first but it reflects the lawn onto the far side of
     * the pavement, which looks wrong. */
    const f32 kEdgeRows = 4.0f;   /* art rows that get stretched into the strip */
    struct Band { int y0, y1; int kind; } bands[3];   /* kind: 0 picture, -1 above, +1 below */
    int nb = 0;
    { int y0 = IMax(clipY, (int)floorf(picTop)), y1 = IMin(clipY + clipH, (int)ceilf(picBottom));
      if (y1 > y0) { bands[nb].y0 = y0; bands[nb].y1 = y1; bands[nb].kind = 0; ++nb; } }
    { int y0 = clipY, y1 = IMin(clipY + clipH, (int)floorf(picTop));
      if (y1 > y0) { bands[nb].y0 = y0; bands[nb].y1 = y1; bands[nb].kind = -1; ++nb; } }
    { int y0 = IMax(clipY, (int)ceilf(picBottom)), y1 = clipY + clipH;
      if (y1 > y0) { bands[nb].y0 = y0; bands[nb].y1 = y1; bands[nb].kind = +1; ++nb; } }

    for (int b = 0; b < nb; ++b)
    {
        /* texture row -> screen y for this band */
        f32 yt, yb;
        if (bands[b].kind == 0)
        {
            yt = picTop + (0.0f - L->cropY) * s;
            yb = picTop + (H - L->cropY) * s;
        }
        else if (bands[b].kind < 0)
        {
            /* rows [cropY, cropY+kEdgeRows] fill the strip [y0, picTop] */
            const f32 k = (picTop - (f32)bands[b].y0) / kEdgeRows;
            yt = (f32)bands[b].y0 + (0.0f - L->cropY) * k;
            yb = (f32)bands[b].y0 + (H - L->cropY) * k;
        }
        else
        {
            /* rows [cropEnd-kEdgeRows, cropEnd] fill the strip [picBottom, y1] */
            const f32 cropEnd = L->cropY + L->cropH;
            const f32 k = ((f32)bands[b].y1 - picBottom) / kEdgeRows;
            yt = picBottom + (0.0f - (cropEnd - kEdgeRows)) * k;
            yb = picBottom + (H - (cropEnd - kEdgeRows)) * k;
        }

        for (int i = i0; i <= i1 && i < i0 + 8; ++i)
        {
            const f32  tileX = L->originX + (f32)i * tw;
            const bool flipX = (i & 1) != 0;
            const int  x0 = IMax(clipX, (int)floorf(tileX));
            const int  x1 = IMin(clipX + clipW, (int)ceilf(tileX + tw));
            if (x1 <= x0) continue;

            const f32 xl = flipX ? tileX + cx1 * s          : tileX + (0.0f - L->cropX) * s;
            const f32 xr = flipX ? tileX + (cx1 - W) * s    : tileX + (W - L->cropX) * s;

            guVector q[4] = { { xl, yt, 0 }, { xr, yt, 0 }, { xr, yb, 0 }, { xl, yb, 0 } };
            GRRLIB_ClipDrawing((u32)x0, (u32)bands[b].y0, (u32)(x1 - x0), (u32)(bands[b].y1 - bands[b].y0));
            GRRLIB_DrawImgQuad(q, tex, 0xFFFFFFFF);
        }
    }
    GRRLIB_ClipReset();
}
