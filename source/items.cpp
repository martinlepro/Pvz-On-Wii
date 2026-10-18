/**
 * items.cpp - Level item rewards: inventory flags + the "New Item!" screen.
 *
 * SELF-CONTAINED: no header changes needed. It only uses Render_* helpers
 * from render.h (re-declared locally below). menu.cpp talks to it through
 * the three ItemAward_* functions (also re-declared locally there).
 *
 * Inventory is stored in its own file, sd:/apps/pvz_wii/items.cfg
 * ("ITEMS=<hex bitmask>"), so Settings / settings.cfg are untouched.
 *
 * Assets (relative to assets/):
 *   background/AwardScreen_Back.png (falls back to background/new_plant.png)
 *   items/Present.png, items/PresentOpen.png
 *   items/shovel.png almanac.png car_key.png taco.png watering_can.png
 *   items/trophy_silver.png trophy_gold.png note.png
 * (rename "car_key..png" -> "car_key.png"; the loader also tries the
 *  double-dot name so nothing breaks if you forget.)
 * note.png is 654x427 is not a multiple of 4 in either axis, so run
 * tools/prepare_textures.py on assets/ (pads it to 656x428) or the note is blank.
 */
#include <grrlib.h>
#include <gccore.h>
#include <wiiuse/wpad.h>
#include <stdio.h>
#include <string.h>
#include <math.h>
#include <sys/stat.h>

#ifndef SCREEN_WIDTH
#define SCREEN_WIDTH 640
#endif
#ifndef SCREEN_HEIGHT
#define SCREEN_HEIGHT 480
#endif
#ifndef SETTINGS_DIR
#define SETTINGS_DIR "sd:/apps/pvz_wii"
#endif
#define ITEMS_PATH SETTINGS_DIR "/items.cfg"

/* ---- re-declared from render.h (C linkage, as render.h declares them) ---- */
extern "C" {
GRRLIB_texImg* Render_LoadTexture(const char* relPath);
void Render_DrawTextCentered(s16 cx, s16 y, u32 color, unsigned int size, const char* text);
int  Render_DrawTextWrapped(s16 x, s16 y, s16 maxWidth, s16 lineHeight, u32 color,
                            unsigned int size, const char* text);
}

enum ItemId {
    ITEM_NONE = -1,
    ITEM_SHOVEL = 0,
    ITEM_NOTE_1, ITEM_NOTE_2, ITEM_NOTE_3, ITEM_NOTE_4, ITEM_NOTE_5,
    ITEM_ALMANAC,
    ITEM_CAR_KEY,
    ITEM_TACO,
    ITEM_WATERING_CAN,
    ITEM_TROPHY_SILVER,
    ITEM_TROPHY_GOLD,
    ITEM_COUNT
};

struct ItemDef {
    const char* name;
    const char* image;      /* path under assets/, NULL = drawn as a note */
    const char* text;       /* one-line description / note body */
};

static const ItemDef kItems[ITEM_COUNT] = {
    { "Shovel",       "assets/items/shovel.png",
      "Dig up any plant you regret planting and get its lawn tile back." },
    { "Crumpled Note 1", NULL,
      "To whoever finds this: the zombies came from the east. They did not knock. Please keep the sunflowers watered." },
    { "Crumpled Note 2", NULL,
      "Day two. The lawn is holding. Someone keeps leaving traffic cones on the porch and I do not like it." },
    { "Crumpled Note 3", NULL,
      "The fog rolled in last night. I could not see the fence, but I could hear the shuffling. Plant something that glows." },
    { "Crumpled Note 4", NULL,
      "Found a pool where my garden used to be. Pea pods do not float. Lily pads do. Write that down." },
    { "Crumpled Note 5", NULL,
      "The roof is the last stand. Whatever happens, thank you for keeping the lawn green." },
    { "Suburban Almanac", "assets/items/almanac.png",
      "A thick field guide full of notes on every plant and zombie you have met." },
    { "Car Key",      "assets/items/car_key.png",
      "A jingly ring of keys. Somebody up the street is going to want these back." },
    { "Taco",         "assets/items/taco.png",
      "A perfectly crunchy taco. Far too important to eat on an ordinary day." },
    { "Watering Can", "assets/items/watering_can.png",
      "Keeps potted plants happy and growing in the Zen Garden." },
    { "Silver Sunflower Trophy", "assets/items/trophy_silver.png",
      "You held the roof! Survival Mode is now unlocked." },
    { "Gold Sunflower Trophy",   "assets/items/trophy_gold.png",
      "Awarded for greatness. Your lawn is the envy of the whole street." },
};

/* Reward table: level index = (world-1)*10 + level-1 */
static int ItemForLevel(int level)
{
    switch (level)
    {
        case 3:  return ITEM_SHOVEL;        /* 1-4  */
        case 8:  return ITEM_NOTE_1;        /* 1-9  */
        case 13: return ITEM_ALMANAC;       /* 2-4  */
        case 18: return ITEM_NOTE_2;        /* 2-9  */
        case 23: return ITEM_CAR_KEY;       /* 3-4  */
        case 28: return ITEM_NOTE_3;        /* 3-9  */
        case 33: return ITEM_TACO;          /* 4-4  */
        case 38: return ITEM_NOTE_4;        /* 4-9  */
        case 43: return ITEM_WATERING_CAN;  /* 5-4  */
        case 48: return ITEM_NOTE_5;        /* 5-9  */
        case 49: return ITEM_TROPHY_SILVER; /* 5-10 */
        default: return ITEM_NONE;
    }
}

/* ------------------------------------------------------------------ */
/* Inventory (persisted)                                               */
/* ------------------------------------------------------------------ */
static u32  s_owned  = 0;
static bool s_loaded = false;

static void LoadInventory(void)
{
    if (s_loaded) return;
    s_loaded = true;
    FILE* f = fopen(ITEMS_PATH, "r");
    if (!f) return;
    char line[64];
    while (fgets(line, sizeof(line), f))
    {
        unsigned v = 0;
        if (sscanf(line, "ITEMS=%x", &v) == 1) s_owned = (u32)v;
    }
    fclose(f);
}

static void SaveInventory(void)
{
    mkdir(SETTINGS_DIR, 0777);
    FILE* f = fopen(ITEMS_PATH, "w");
    if (!f) return; /* no SD: keep it in memory for this session */
    fprintf(f, "ITEMS=%x\n", (unsigned)s_owned);
    fclose(f);
}

extern "C" bool Items_Has(int item)
{
    LoadInventory();
    return item >= 0 && item < ITEM_COUNT && (s_owned & (1u << item));
}

/** Convenience for other files: is the shovel owned? (HUD step) */
extern "C" bool Items_HasShovel(void)       { return Items_Has(ITEM_SHOVEL); }
/** Survival Mode is unlocked by the silver trophy (5-10). */
extern "C" bool Items_SurvivalUnlocked(void){ return Items_Has(ITEM_TROPHY_SILVER); }

extern "C" void Items_Grant(int item)
{
    LoadInventory();
    if (item < 0 || item >= ITEM_COUNT) return;
    s_owned |= (1u << item);
    SaveInventory();
}

/* ------------------------------------------------------------------ */
/* Award screen                                                         */
/* ------------------------------------------------------------------ */
static GRRLIB_texImg* s_bg      = NULL;
static GRRLIB_texImg* s_present = NULL;
static GRRLIB_texImg* s_open    = NULL;
static GRRLIB_texImg* s_note    = NULL;
static GRRLIB_texImg* s_itemTex = NULL;
static int  s_texFor  = -2;   /* item whose s_itemTex is loaded */
static bool s_assets  = false;
static int  s_phase   = 0;    /* 0 closed gift, 1 opening, 2 reveal */
static u32  s_frame   = 0;
static u32  s_phaseStart = 0;
static int  s_activeItem = ITEM_NONE;

#define AW_BTN_X 240
#define AW_BTN_Y 404
#define AW_BTN_W 160
#define AW_BTN_H 42

static void DrawTex(GRRLIB_texImg* t, f32 cx, f32 cy, f32 sx, f32 sy, u32 color)
{
    if (!t || !t->data) return;
    f32 hw = t->w * sx * 0.5f, hh = t->h * sy * 0.5f;
    guVector q[4] = {
        { cx - hw, cy - hh, 0.0f }, { cx + hw, cy - hh, 0.0f },
        { cx + hw, cy + hh, 0.0f }, { cx - hw, cy + hh, 0.0f } };
    GRRLIB_DrawImgQuad(q, t, color);
}

static GRRLIB_texImg* LoadItemImage(int item)
{
    const char* p = kItems[item].image;
    if (!p) return NULL;
    GRRLIB_texImg* t = Render_LoadTexture(p);
    if (!t && item == ITEM_CAR_KEY)
        t = Render_LoadTexture("assets/items/car_key..png"); /* original typo */
    return t;
}

static void EnsureAssets(int item)
{
    if (!s_assets)
    {
        s_assets  = true;
        s_bg      = Render_LoadTexture("assets/background/AwardScreen_Back.png");
        if (!s_bg) s_bg = Render_LoadTexture("assets/background/new_plant.png");
        s_present = Render_LoadTexture("assets/items/Present.png");
        s_open    = Render_LoadTexture("assets/items/PresentOpen.png");
    }
    if (s_texFor != item)
    {
        if (s_itemTex) { GRRLIB_FreeTexture(s_itemTex); s_itemTex = NULL; }
        if (s_note)    { GRRLIB_FreeTexture(s_note);    s_note    = NULL; }
        if (kItems[item].image) s_itemTex = LoadItemImage(item);
        else                    s_note    = Render_LoadTexture("assets/items/note.png");
        s_texFor = item;
    }
}

static void FreeAssets(void)
{
    GRRLIB_texImg** all[] = { &s_bg, &s_present, &s_open, &s_note, &s_itemTex };
    for (unsigned i = 0; i < sizeof(all) / sizeof(all[0]); ++i)
        if (*all[i]) { GRRLIB_FreeTexture(*all[i]); *all[i] = NULL; }
    s_assets = false;
    s_texFor = -2;
    s_phase  = 0;
    s_activeItem = ITEM_NONE;
}

/** True while this level still owes the player an unclaimed item. */
extern "C" bool ItemAward_Active(unsigned char levelIndex)
{
    int item = ItemForLevel(levelIndex);
    return item != ITEM_NONE && !Items_Has(item);
}

/** Call every frame while ItemAward_Active(). down = WPAD_ButtonsDown(0).
 *  Returns true on the frame the item is claimed (then the normal victory
 *  menu takes over, because ItemAward_Active() is now false). */
extern "C" bool ItemAward_Update(unsigned char levelIndex, u32 down,
                                 bool pointerValid, f32 px, f32 py)
{
    int item = ItemForLevel(levelIndex);
    if (item == ITEM_NONE) return false;
    if (s_activeItem != item)
    {
        s_activeItem = item; s_phase = 0; s_frame = 0; s_phaseStart = 0;
    }
    ++s_frame;
    EnsureAssets(item);

    bool pressed = (down & (WPAD_BUTTON_A | WPAD_BUTTON_B)) != 0;
    (void)pointerValid; (void)px; (void)py; /* A/B anywhere; button is just a hint */

    if (s_phase == 0)
    {
        if (pressed) { s_phase = 1; s_phaseStart = s_frame; }
    }
    else if (s_phase == 1)
    {
        if (s_frame - s_phaseStart > 36) { s_phase = 2; s_phaseStart = s_frame; }
    }
    else if (pressed && s_frame - s_phaseStart > 12)
    {
        Items_Grant(item);   /* saves to SD */
        FreeAssets();
        return true;
    }
    return false;
}

extern "C" void ItemAward_Render(unsigned char levelIndex, bool pointerValid, f32 px, f32 py)
{
    int item = ItemForLevel(levelIndex);
    if (item == ITEM_NONE) return;
    EnsureAssets(item);

    const u32 kShadow = 0x2E1608FF, kCream = 0xFFF1C9FF;
    const bool isNote = (kItems[item].image == NULL);

    if (s_bg && s_bg->data)
    {
        guVector q[4] = { {0,0,0}, {(f32)SCREEN_WIDTH,0,0},
                          {(f32)SCREEN_WIDTH,(f32)SCREEN_HEIGHT,0}, {0,(f32)SCREEN_HEIGHT,0} };
        GRRLIB_DrawImgQuad(q, s_bg, 0xFFFFFFFF);
    }
    else
        GRRLIB_Rectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0xC98B4EFF, true);

    const char* title = (s_phase == 2) ? (isNote ? "You found a note!" : "New Item!")
                                       : "You found a gift!";
    Render_DrawTextCentered(SCREEN_WIDTH / 2 + 2, 24, kShadow, 28, title);
    Render_DrawTextCentered(SCREEN_WIDTH / 2,     22, 0xFFE27AFF, 28, title);

    const f32 cx = 320.0f, cy = 160.0f;   /* centre of the picture window */
    f32 bob = sinf((f32)s_frame * 0.09f) * 4.0f;

    if (s_phase == 0)
    {
        DrawTex(s_present, cx, cy + bob, 1.6f, 1.6f, 0xFFFFFFFF);
        Render_DrawTextCentered(SCREEN_WIDTH / 2, 244, kCream, 22, "A mysterious gift");
        Render_DrawTextWrapped(214, 290, 212, 18, 0x4A2A10FF, 15, "Press A to open it.");
    }
    else if (s_phase == 1)
    {
        f32 t = (f32)(s_frame - s_phaseStart) / 36.0f;            /* 0..1 */
        DrawTex(s_open, cx, cy + 20, 1.4f, 1.4f, 0xFFFFFFFF);
        if (!isNote && s_itemTex && s_itemTex->data)
        {
            f32 sc = 0.2f + 0.8f * t;
            f32 fit = 120.0f / (s_itemTex->h > s_itemTex->w ? s_itemTex->h : s_itemTex->w);
            DrawTex(s_itemTex, cx, cy + 20 - 70.0f * t, fit * sc, fit * sc, 0xFFFFFFFF);
        }
    }
    else
    {
        if (!isNote)
        {
            if (s_itemTex && s_itemTex->data)
            {
                f32 fit = 120.0f / (s_itemTex->h > s_itemTex->w ? s_itemTex->h : s_itemTex->w);
                DrawTex(s_itemTex, cx, cy + bob * 0.5f, fit, fit, 0xFFFFFFFF);
            }
            else /* missing art: coloured placeholder, never crash */
                GRRLIB_Rectangle((int)cx - 40, (int)cy - 40, 80, 80, 0xE0B040FF, true);
            Render_DrawTextCentered(SCREEN_WIDTH / 2 + 1, 245, kShadow, 22, kItems[item].name);
            Render_DrawTextCentered(SCREEN_WIDTH / 2,     244, kCream,  22, kItems[item].name);
            Render_DrawTextWrapped(214, 290, 212, 18, 0x4A2A10FF, 15, kItems[item].text);
        }
        else
        {
            /* Readable note on top of the card: 654x427 scaled to ~0.78 */
            const f32 ncx = 320.0f, ncy = 215.0f, sc = 0.78f;
            if (s_note && s_note->data)
                DrawTex(s_note, ncx, ncy, sc, sc, 0xFFFFFFFF);
            else
                GRRLIB_Rectangle(70, 60, 500, 320, 0xF8EAA8FF, true);
            Render_DrawTextCentered(SCREEN_WIDTH / 2, 82, 0x3A2A10FF, 20, kItems[item].name);
            Render_DrawTextWrapped(120, 120, 400, 22, 0x3A2A10FF, 17, kItems[item].text);
        }
    }

    if (s_phase == 2)
    {
        GRRLIB_Rectangle(AW_BTN_X - 3, AW_BTN_Y - 3, AW_BTN_W + 6, AW_BTN_H + 6, 0xFFE27AFF, true);
        GRRLIB_Rectangle(AW_BTN_X, AW_BTN_Y, AW_BTN_W, AW_BTN_H, 0xB8662CFF, true);
        Render_DrawTextCentered(AW_BTN_X + AW_BTN_W / 2, AW_BTN_Y + 10, 0xFFFFFFFF, 22, "Continue");
    }

    /* The full-screen art covers the normal reticle, so draw a small one. */
    if (pointerValid)
    {
        GRRLIB_Circle(px, py, 9, 0x000000FF, false);
        GRRLIB_Circle(px, py, 7, 0xFFFFFFFF, false);
    }
}
