/**
 * zen.cpp - Zen Garden (minimal version), reached from the Puzzle menu.
 *
 * SELF-CONTAINED: own state + renderer; reads/writes coins through hud.cpp's
 * Coins_* (step 4) and saves its plants to sd:/apps/pvz_wii/zen.cfg.
 *
 * What you can do (point + click with A/B):
 *   * Shop ($): buy a seedling for coins; it goes into the wheelbarrow.
 *   * Wheelbarrow: with a plant in hand, click it to stash the plant; with an
 *     empty hand, click it to take the next stashed plant out.
 *   * Glove: pick a plant up from a slot, click an empty slot to put it down
 *     (swaps with an occupied slot). Drop it on the $ to sell it.
 *   * Watering can: water a thirsty plant (blue drop). Each watering grows it
 *     one stage; once fully grown, watering pays out coins (silver / gold).
 *   * Arrow: switch between the Main Garden (32 slots) and Mushroom Garden (8).
 *   HOME returns to the Puzzle menu.
 *
 * Assets: ui/zen_garden_main.png + ui/zen_garden_mushroom.png (derived),
 * ui/zen_reticle.png + ui/zen_wheelbarrow.png (derived, padded),
 * ui/zen_glove.png, ui/zen_MoneySign.png, ui/zen_NextGarden.png,
 * items/watering_can.png, ui/coinbank_small.png (step 4). Missing art falls back
 * to flat shapes.
 * Tools: tools/make_zen_assets.py derives the padded/cropped PNGs.
 */
#include "input.h"
#include "constants.h"
#include "types.h"
#include <grrlib.h>
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <math.h>
#include <sys/stat.h>

extern "C" {
GRRLIB_texImg* Render_LoadTexture(const char* relPath);
void Render_DrawText(s16 x, s16 y, u32 color, unsigned int size, const char* text);
void Render_DrawTextCentered(s16 cx, s16 y, u32 color, unsigned int size, const char* text);
void Render_DrawPlantPortrait(PlantType type, s16 cx, s16 bottomY, s16 targetH, s16 maxW, u32 frameCount);
unsigned Coins_Get(void);          /* hud.cpp (step 4) */
bool     Coins_Spend(unsigned n);
void     Coins_Add(unsigned n);
bool     Items_Has(int item);      /* items.cpp (step 1) */
}

#ifndef SETTINGS_DIR
#define SETTINGS_DIR "sd:/apps/pvz_wii"
#endif
#define ZEN_CFG SETTINGS_DIR "/zen.cfg"
#define ZEN_REQUIRES_CAN 0     /* 1 = Zen Garden only opens after the 5-4 Watering Can reward */
#define ITEM_WATERING_CAN_ID 9 /* ItemId in items.cpp */

#define THIRST_FRAMES (60 * 35)
#define STAGES 4               /* 0 sprout .. 3 fully grown */
#define MAX_SLOTS 32
#define STASH_MAX 6
#define GARDENS 2

/* ------------------------------------------------------------------ */
static GRRLIB_texImg *s_bg[GARDENS], *s_glove, *s_money, *s_next, *s_can, *s_barrow, *s_reticle, *s_bank;
static bool s_loaded = false;

static const f32 SC = 2.5f;                       /* garden art 256x192 -> 640x480 */
static const int kMainX[8] = { 34, 60, 86, 112, 147, 173, 199, 225 };
static const int kMainY[4] = { 48, 80, 111, 143 };
static const int kMushX[8] = { 133, 126, 188, 88, 156, 46, 106, 170 };
static const int kMushY[8] = { 84, 112, 110, 136, 142, 160, 168, 170 };
static int SlotCount(int g) { return g == 0 ? 32 : 8; }
static void SlotPos(int g, int i, f32* x, f32* y)
{
    if (g == 0) { *x = kMainX[i % 8] * SC; *y = kMainY[i / 8] * SC; }
    else        { *x = kMushX[i] * SC;      *y = kMushY[i] * SC; }
}

struct Seedling { s8 type; u8 stage; };          /* type -1 = empty */
struct Slot { Seedling s; u16 thirst; bool thirsty; };

static Slot     s_slot[GARDENS][MAX_SLOTS];
static Seedling s_stash[STASH_MAX]; static int s_nstash;
static Seedling s_hand;                           /* glove-carried plant (type -1 = none) */
static int      s_handFromG = -1, s_handFromI = -1;
static int      s_garden = 0;
static enum { T_CAN, T_GLOVE } s_tool = T_CAN;
static bool     s_shop = false;
static u32      s_frame = 0;

struct Pop { bool a; f32 x, y; u8 t; int amount; };
static Pop s_pop[8];

struct ShopItem { PlantType type; const char* name; unsigned price; };
static const ShopItem kShop[8] = {
    { PLANT_PUFF_SHROOM, "Puff-shroom", 30 }, { PLANT_PEASHOOTER, "Peashooter", 50 },
    { PLANT_SUNFLOWER, "Sunflower", 50 },     { PLANT_WALLNUT, "Wall-nut", 75 },
    { PLANT_SUN_SHROOM, "Sun-shroom", 75 },   { PLANT_SNOW_PEA, "Snow Pea", 100 },
    { PLANT_CHOMPER, "Chomper", 150 },        { PLANT_REPEATER, "Repeater", 200 } };

static unsigned PriceOf(int type)
{ for (int i = 0; i < 8; ++i) if (kShop[i].type == type) return kShop[i].price; return 50; }

/* ---- layout of the toolbar ---- */
static const int TB_Y = 408, TB_S = 56;
static const int CAN_X = 14, GLOVE_X = 76, BARROW_X = 138, MONEY_X = 200;
static const int NEXT_X = 574, NEXT_Y = 330, NEXT_S = 48;

/* ------------------------------------------------------------------ */
static void Quad(GRRLIB_texImg* t, f32 x, f32 y, f32 w, f32 h, u32 color)
{
    if (!t || !t->data) return;
    guVector q[4] = { { x, y, 0 }, { x + w, y, 0 }, { x + w, y + h, 0 }, { x, y + h, 0 } };
    GRRLIB_DrawImgQuad(q, t, color);
}
static bool In(f32 px, f32 py, int x, int y, int w, int h)
{ return px >= x && px < x + w && py >= y && py < y + h; }

static void Save(void)
{
    mkdir(SETTINGS_DIR, 0777);
    FILE* f = fopen(ZEN_CFG, "w");
    if (!f) return;
    for (int g = 0; g < GARDENS; ++g)
        for (int i = 0; i < SlotCount(g); ++i)
            if (s_slot[g][i].s.type >= 0) fprintf(f, "S %d %d %d %d\n", g, i, s_slot[g][i].s.type, s_slot[g][i].s.stage);
    for (int i = 0; i < s_nstash; ++i) fprintf(f, "W %d %d\n", s_stash[i].type, s_stash[i].stage);
    fprintf(f, "G %d\n", s_garden);
    fclose(f);
}

static void Load(void)
{
    for (int g = 0; g < GARDENS; ++g)
        for (int i = 0; i < MAX_SLOTS; ++i)
        { s_slot[g][i].s.type = -1; s_slot[g][i].s.stage = 0; s_slot[g][i].thirst = 0; s_slot[g][i].thirsty = true; }
    s_nstash = 0; s_hand.type = -1; s_handFromG = s_handFromI = -1;
    FILE* f = fopen(ZEN_CFG, "r");
    if (!f) return;
    char line[64];
    while (fgets(line, sizeof(line), f))
    {
        int g, i, t, st;
        if (sscanf(line, "S %d %d %d %d", &g, &i, &t, &st) == 4 && g >= 0 && g < GARDENS && i >= 0 && i < SlotCount(g)
            && t >= 0 && t < PLANT_TYPE_COUNT && st >= 0 && st < STAGES)
        { s_slot[g][i].s.type = (s8)t; s_slot[g][i].s.stage = (u8)st; }
        else if (sscanf(line, "W %d %d", &t, &st) == 2 && s_nstash < STASH_MAX && t >= 0 && t < PLANT_TYPE_COUNT)
        { s_stash[s_nstash].type = (s8)t; s_stash[s_nstash].stage = (u8)(st < STAGES ? st : 0); s_nstash++; }
        else if (sscanf(line, "G %d", &g) == 1 && g >= 0 && g < GARDENS) s_garden = g;
    }
    fclose(f);
}

extern "C" void Zen_Free(void)
{
    GRRLIB_texImg** all[] = { &s_bg[0], &s_bg[1], &s_glove, &s_money, &s_next, &s_can, &s_barrow, &s_reticle, &s_bank };
    for (unsigned i = 0; i < sizeof(all) / sizeof(all[0]); ++i)
        if (*all[i]) { GRRLIB_FreeTexture(*all[i]); *all[i] = NULL; }
    s_loaded = false;
}

/** Returns false if the mode is locked (only when ZEN_REQUIRES_CAN is 1). */
extern "C" bool Zen_Open(void)
{
#if ZEN_REQUIRES_CAN
    if (!Items_Has(ITEM_WATERING_CAN_ID)) return false;
#endif
    if (!s_loaded)
    {
        s_loaded = true;
        s_bg[0]    = Render_LoadTexture("assets/ui/zen_garden_main.png");
        s_bg[1]    = Render_LoadTexture("assets/ui/zen_garden_mushroom.png");
        s_glove    = Render_LoadTexture("assets/ui/zen_glove.png");
        s_money    = Render_LoadTexture("assets/ui/zen_MoneySign.png");
        s_next     = Render_LoadTexture("assets/ui/zen_NextGarden.png");
        s_can      = Render_LoadTexture("assets/items/watering_can.png");
        s_barrow   = Render_LoadTexture("assets/ui/zen_wheelbarrow.png");
        s_reticle  = Render_LoadTexture("assets/ui/zen_reticle.png");
        s_bank     = Render_LoadTexture("assets/ui/coinbank_small.png");
    }
    Load();
    s_tool = T_CAN; s_shop = false; s_frame = 0;
    memset(s_pop, 0, sizeof(s_pop));
    return true;
}

static void AddPop(f32 x, f32 y, int amount)
{ for (int i = 0; i < 8; ++i) if (!s_pop[i].a) { s_pop[i].a = true; s_pop[i].x = x; s_pop[i].y = y; s_pop[i].t = 0; s_pop[i].amount = amount; return; } }

static int SlotAt(f32 px, f32 py)
{
    for (int i = 0; i < SlotCount(s_garden); ++i)
    {
        f32 x, y; SlotPos(s_garden, i, &x, &y);
        if (fabsf(px - x) < 32 && py > y - 40 && py < y + 28) return i;
    }
    return -1;
}

static void ReturnHand(void)
{
    if (s_hand.type < 0) return;
    if (s_handFromG >= 0 && s_slot[s_handFromG][s_handFromI].s.type < 0)
        s_slot[s_handFromG][s_handFromI].s = s_hand;
    else if (s_nstash < STASH_MAX) s_stash[s_nstash++] = s_hand;
    s_hand.type = -1; s_handFromG = s_handFromI = -1;
}

/** Returns true when the player wants to leave Zen Garden. */
extern "C" bool Zen_Update(const InputState* in, unsigned wpadDown)
{
    s_frame++;
    if (wpadDown & 0x0080) { ReturnHand(); Save(); return true; }   /* HOME */

    /* thirst timers */
    for (int g = 0; g < GARDENS; ++g)
        for (int i = 0; i < SlotCount(g); ++i)
        {
            Slot* sl = &s_slot[g][i];
            if (sl->s.type >= 0 && !sl->thirsty && ++sl->thirst >= THIRST_FRAMES) sl->thirsty = true;
        }
    for (int i = 0; i < 8; ++i) if (s_pop[i].a && ++s_pop[i].t > 50) s_pop[i].a = false;

    if (!in->cursorValid || !in->placePressed) return false;
    const f32 px = in->cursor.x, py = in->cursor.y;

    /* Shop panel is modal */
    if (s_shop)
    {
        if (In(px, py, 424, 70, 150, 34)) { s_shop = false; return false; }          /* Close */
        for (int i = 0; i < 8; ++i)
            if (In(px, py, 90, 96 + i * 38, 330, 34))
            {
                if (s_nstash >= STASH_MAX) { AddPop(px, py, 0); return false; }
                if (Coins_Spend(kShop[i].price))
                { s_stash[s_nstash].type = (s8)kShop[i].type; s_stash[s_nstash].stage = 0; s_nstash++; Save(); }
                else AddPop(px, py, 0);
                return false;
            }
        return false;
    }

    /* Toolbar */
    if (In(px, py, CAN_X, TB_Y, TB_S, TB_S))   { ReturnHand(); s_tool = T_CAN; return false; }
    if (In(px, py, GLOVE_X, TB_Y, TB_S, TB_S)) { s_tool = T_GLOVE; return false; }
    if (In(px, py, BARROW_X, TB_Y, TB_S, TB_S))
    {
        if (s_hand.type >= 0) { if (s_nstash < STASH_MAX) { s_stash[s_nstash++] = s_hand; s_hand.type = -1; s_handFromG = -1; Save(); } }
        else if (s_nstash > 0) { s_hand = s_stash[--s_nstash]; s_handFromG = s_handFromI = -1; s_tool = T_GLOVE; Save(); }
        return false;
    }
    if (In(px, py, MONEY_X, TB_Y, TB_S, TB_S))
    {
        if (s_hand.type >= 0)   /* sell the plant in hand */
        {
            unsigned v = PriceOf(s_hand.type) / 2 + 12u * s_hand.stage;
            Coins_Add(v); AddPop(MONEY_X + 28, TB_Y - 6, (int)v);
            s_hand.type = -1; s_handFromG = -1; Save();
        }
        else s_shop = true;
        return false;
    }
    if (In(px, py, NEXT_X, NEXT_Y, NEXT_S, NEXT_S)) { ReturnHand(); s_garden = (s_garden + 1) % GARDENS; Save(); return false; }

    /* Garden slots */
    int i = SlotAt(px, py);
    if (i < 0) return false;
    Slot* sl = &s_slot[s_garden][i];
    f32 sx, sy; SlotPos(s_garden, i, &sx, &sy);

    if (s_tool == T_CAN)
    {
        if (sl->s.type >= 0 && sl->thirsty)
        {
            sl->thirsty = false; sl->thirst = 0;
            if (sl->s.stage + 1 < STAGES) sl->s.stage++;
            else
            {
                int v = (rand() % 5 == 0) ? 50 : 10;      /* gold coin sometimes */
                Coins_Add((unsigned)v); AddPop(sx, sy - 50, v);
            }
            Save();
        }
    }
    else /* glove */
    {
        if (s_hand.type < 0)
        {
            if (sl->s.type >= 0) { s_hand = sl->s; sl->s.type = -1; s_handFromG = s_garden; s_handFromI = i; }
        }
        else
        {
            Seedling prev = sl->s;
            sl->s = s_hand; sl->thirst = 0; sl->thirsty = true;
            if (prev.type >= 0) { s_hand = prev; s_handFromG = s_garden; s_handFromI = i; }
            else { s_hand.type = -1; s_handFromG = s_handFromI = -1; }
            Save();
        }
    }
    return false;
}

/* ------------------------------------------------------------------ */
static void DrawPlant(const Seedling* p, f32 x, f32 y, bool thirsty)
{
    static const s16 kH[STAGES] = { 26, 38, 50, 62 };
    Render_DrawPlantPortrait((PlantType)p->type, (s16)x, (s16)(y + 12), kH[p->stage], 56, s_frame);
    if (thirsty)
    {
        const f32 b = sinf((f32)s_frame * 0.15f) * 2.0f;
        if (p->stage == STAGES - 1) GRRLIB_Circle(x + 16, y - kH[p->stage] + b, 7, 0xFFD34DFF, true);
        else                        GRRLIB_Circle(x + 16, y - kH[p->stage] + b, 6, 0x4FB0FFFF, true);
    }
}

static void Tool(GRRLIB_texImg* t, int x, bool sel, u32 fallback)
{
    GRRLIB_Rectangle(x - 2, TB_Y - 2, TB_S + 4, TB_S + 4, sel ? 0xFFE27AFF : 0x2E1608FF, true);
    GRRLIB_Rectangle(x, TB_Y, TB_S, TB_S, 0x6B3410FF, true);
    if (t && t->data) Quad(t, x + 2, TB_Y + 2, TB_S - 4, TB_S - 4, 0xFFFFFFFF);
    else GRRLIB_Rectangle(x + 10, TB_Y + 10, TB_S - 20, TB_S - 20, fallback, true);
}

extern "C" void Zen_Render(const InputState* in)
{
    if (s_bg[s_garden] && s_bg[s_garden]->data) Quad(s_bg[s_garden], 0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0xFFFFFFFF);
    else GRRLIB_Rectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0x4A8C3AFF, true);

    /* plants, back rows first */
    for (int i = 0; i < SlotCount(s_garden); ++i)
    {
        const Slot* sl = &s_slot[s_garden][i];
        if (sl->s.type < 0) continue;
        f32 x, y; SlotPos(s_garden, i, &x, &y);
        DrawPlant(&sl->s, x, y, sl->thirsty);
    }

    /* toolbar */
    GRRLIB_Rectangle(0, 400, SCREEN_WIDTH, 80, 0x2E1608D0, true);
    Tool(s_can, CAN_X, s_tool == T_CAN && s_hand.type < 0, 0x2E9B5EFF);
    Tool(s_glove, GLOVE_X, s_tool == T_GLOVE, 0x3DAA3DFF);
    Tool(s_barrow, BARROW_X, false, 0xB03030FF);
    Tool(s_money, MONEY_X, false, 0xE0B020FF);
    char buf[40];
    snprintf(buf, sizeof(buf), "x%d", s_nstash);
    Render_DrawText(BARROW_X + 34, TB_Y + 38, 0xFFFFFFFF, 14, buf);
    if (s_next && s_next->data) Quad(s_next, NEXT_X, NEXT_Y, NEXT_S, NEXT_S, 0xFFFFFFFF);
    else GRRLIB_Rectangle(NEXT_X, NEXT_Y, NEXT_S, NEXT_S, 0xFFFFFF80, true);

    /* coin bank */
    const int bx = SCREEN_WIDTH - 118, by = 424;
    if (s_bank && s_bank->data) Quad(s_bank, bx, by, 104, 24, 0xFFFFFFFF);
    else GRRLIB_Rectangle(bx, by, 104, 24, 0x8A4A1EFF, true);
    snprintf(buf, sizeof(buf), "%u", Coins_Get());
    Render_DrawTextCentered(bx + 68, by + 4, 0xFFE27AFF, 14, buf);
    Render_DrawTextCentered(SCREEN_WIDTH / 2 + 60, 428, 0xFFFFFFFF, 13,
                            s_tool == T_CAN ? "Water thirsty plants" : "Glove: move plants");

    /* shop */
    if (s_shop)
    {
        GRRLIB_Rectangle(70, 60, 450, 340, 0x000000C0, true);
        GRRLIB_Rectangle(80, 70, 330, 28, 0x8A4A1EFF, true);
        Render_DrawText(90, 76, 0xFFE27AFF, 16, "Crazy shop - buy a seedling");
        for (int i = 0; i < 8; ++i)
        {
            bool ok = Coins_Get() >= kShop[i].price && s_nstash < STASH_MAX;
            GRRLIB_Rectangle(90, 96 + i * 38, 330, 34, ok ? 0x4C7A2AFF : 0x555555FF, true);
            Render_DrawText(100, 105 + i * 38, 0xFFFFFFFF, 15, kShop[i].name);
            snprintf(buf, sizeof(buf), "%u", kShop[i].price);
            Render_DrawText(350, 105 + i * 38, 0xFFE27AFF, 15, buf);
        }
        GRRLIB_Rectangle(424, 70, 90, 34, 0xB03030FF, true);
        Render_DrawTextCentered(469, 79, 0xFFFFFFFF, 15, "Close");
    }

    /* pops */
    for (int i = 0; i < 8; ++i)
        if (s_pop[i].a)
        {
            if (s_pop[i].amount > 0) snprintf(buf, sizeof(buf), "+%d", s_pop[i].amount);
            else snprintf(buf, sizeof(buf), "no room / coins");
            Render_DrawTextCentered((s16)s_pop[i].x, (s16)(s_pop[i].y - s_pop[i].t * 0.6f),
                                    s_pop[i].amount > 0 ? 0xFFE27AFF : 0xFF8080FF, 16, buf);
        }

    /* pointer-held items */
    if (in->cursorValid)
    {
        if (s_hand.type >= 0)
            Render_DrawPlantPortrait((PlantType)s_hand.type, (s16)in->cursor.x, (s16)(in->cursor.y + 24), 50, 52, s_frame);
        else if (s_tool == T_CAN && s_can && s_can->data)
            Quad(s_can, in->cursor.x - 12, in->cursor.y - 40, 52, 52, 0xFFFFFFFF);
        else if (s_tool == T_GLOVE && s_glove && s_glove->data)
            Quad(s_glove, in->cursor.x - 12, in->cursor.y - 20, 44, 44, 0xFFFFFFFF);
    }
}
