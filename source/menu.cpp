/**
 * menu.cpp - Pause/options menu: volume slider + control remap, persisted
 * via settings.cpp. See menu.h for the navigation model.
 */
#include "menu.h"
#include "constants.h"
#include "render.h"

#include <grrlib.h>
#include <stdio.h>
#include <string.h>
#include <wiiuse/wpad.h>

#define MAIN_ITEM_COUNT     4
#define CONTROLS_ITEM_COUNT 7 /* 6 remappable bindings + "Back" */

static const char* kMainItems[MAIN_ITEM_COUNT] = { "Volume", "Controls", "Reset Lawn", "Close" };
static const char* kControlLabels[6] = {
    "Place (primary)", "Place (secondary)",
    "Move Right",      "Move Left",
    "Menu (primary)",  "Menu (secondary)"
};

/* -------------------------------------------------------------------------- */
/* Binding slot helpers                                                       */
/* -------------------------------------------------------------------------- */

static u32* BindingSlot(Settings* settings, int index)
{
    switch (index)
    {
        case 0: return &settings->keyPlacePrimary;
        case 1: return &settings->keyPlaceSecondary;
        case 2: return &settings->keyMoveRight;
        case 3: return &settings->keyMoveLeft;
        case 4: return &settings->keyMenuPrimary;
        case 5: return &settings->keyMenuSecondary;
        default: return NULL;
    }
}

static u32 BindingValue(const Settings* settings, int index)
{
    switch (index)
    {
        case 0: return settings->keyPlacePrimary;
        case 1: return settings->keyPlaceSecondary;
        case 2: return settings->keyMoveRight;
        case 3: return settings->keyMoveLeft;
        case 4: return settings->keyMenuPrimary;
        case 5: return settings->keyMenuSecondary;
        default: return 0;
    }
}

/* -------------------------------------------------------------------------- */
/* Public API - state machine                                                 */
/* -------------------------------------------------------------------------- */

void Menu_Init(MenuContext* menu)
{
    if (!menu)
        return;
    memset(menu, 0, sizeof(*menu));
    menu->screen = MENU_CLOSED;
}

bool Menu_IsOpen(const MenuContext* menu)
{
    return menu && menu->screen != MENU_CLOSED;
}

void Menu_Toggle(MenuContext* menu, Settings* settings)
{
    if (!menu)
        return;

    if (menu->screen == MENU_CLOSED)
    {
        menu->screen = MENU_MAIN;
        menu->cursorIndex = 0;
    }
    else
    {
        menu->screen = MENU_CLOSED;
        Settings_Save(settings); /* persist volume + bindings on close */
    }
}

void Menu_HandleInput(MenuContext* menu, Settings* settings, GameContext* game)
{
    if (!menu || menu->screen == MENU_CLOSED || !settings)
        return;

    u32 down = WPAD_ButtonsDown(0);

    if (menu->screen == MENU_REBIND_WAIT)
    {
        int count = Settings_RemappableButtonCount();
        for (int i = 0; i < count; ++i)
        {
            u32 candidate = Settings_RemappableButtonAt(i);
            if (down & candidate)
            {
                u32* slot = BindingSlot(settings, menu->rebindActionIndex);
                if (slot)
                    *slot = candidate;
                menu->screen = MENU_CONTROLS;
                return;
            }
        }
        return; /* still waiting for a button */
    }

    if (down & WPAD_BUTTON_UP)
        menu->cursorIndex--;
    if (down & WPAD_BUTTON_DOWN)
        menu->cursorIndex++;

    int itemCount = 1;
    if (menu->screen == MENU_MAIN)     itemCount = MAIN_ITEM_COUNT;
    if (menu->screen == MENU_CONTROLS) itemCount = CONTROLS_ITEM_COUNT;

    if (menu->cursorIndex < 0)
        menu->cursorIndex = itemCount - 1;
    if (menu->cursorIndex >= itemCount)
        menu->cursorIndex = 0;

    switch (menu->screen)
    {
        case MENU_MAIN:
            if (down & WPAD_BUTTON_A)
            {
                switch (menu->cursorIndex)
                {
                    case 0: menu->screen = MENU_VOLUME; break;
                    case 1: menu->screen = MENU_CONTROLS; menu->cursorIndex = 0; break;
                    case 2: if (game) Game_Reset(game); break;
                    case 3: Menu_Toggle(menu, settings); break;
                }
            }
            else if (down & WPAD_BUTTON_B)
            {
                Menu_Toggle(menu, settings);
            }
            break;

        case MENU_VOLUME:
            if (down & WPAD_BUTTON_LEFT)
                settings->volume = (settings->volume >= 5) ? (u8)(settings->volume - 5) : 0;
            if (down & WPAD_BUTTON_RIGHT)
                settings->volume = (settings->volume <= 95) ? (u8)(settings->volume + 5) : 100;
            if (down & (WPAD_BUTTON_A | WPAD_BUTTON_B))
            {
                menu->screen = MENU_MAIN;
                menu->cursorIndex = 0;
            }
            break;

        case MENU_CONTROLS:
            if (down & WPAD_BUTTON_A)
            {
                if (menu->cursorIndex == CONTROLS_ITEM_COUNT - 1)
                {
                    menu->screen = MENU_MAIN;
                    menu->cursorIndex = 1;
                }
                else
                {
                    menu->rebindActionIndex = menu->cursorIndex;
                    menu->screen = MENU_REBIND_WAIT;
                }
            }
            else if (down & WPAD_BUTTON_B)
            {
                menu->screen = MENU_MAIN;
                menu->cursorIndex = 1;
            }
            break;

        default:
            break;
    }
}

/* -------------------------------------------------------------------------- */
/* Rendering                                                                  */
/* -------------------------------------------------------------------------- */

static void DrawText(s16 x, s16 y, u32 color, const char* text)
{
    GRRLIB_ttfFont* font = Render_GetFont();
    if (font)
        GRRLIB_PrintfTTF(x, y, font, text, 16, color);
}

void Menu_Render(const MenuContext* menu, const Settings* settings)
{
    if (!menu || menu->screen == MENU_CLOSED)
        return;

    /* Dim the gameplay frame behind the panel. */
    GRRLIB_Rectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0x000000A0, true);

    s16 panelX = (SCREEN_WIDTH - MENU_PANEL_W) / 2;
    s16 panelY = (SCREEN_HEIGHT - MENU_PANEL_H) / 2;

    GRRLIB_Rectangle(panelX, panelY, MENU_PANEL_W, MENU_PANEL_H, 0x303030EE, true);
    GRRLIB_Rectangle(panelX, panelY, MENU_PANEL_W, MENU_PANEL_H, 0xFFFFFFFF, false);

    s16 x = panelX + 20;
    s16 y = panelY + 16;

    switch (menu->screen)
    {
        case MENU_MAIN:
        {
            DrawText(x, y, 0xFFFFFFFF, "Options");
            y += 34;
            for (int i = 0; i < MAIN_ITEM_COUNT; ++i)
            {
                bool sel = (i == menu->cursorIndex);
                if (sel)
                    GRRLIB_Rectangle(x - 8, y - 4, MENU_PANEL_W - 24, MENU_ITEM_H, 0x4444AAFF, true);
                DrawText(x, y, sel ? 0xFFFF00FF : 0xFFFFFFFF, kMainItems[i]);
                y += MENU_ITEM_H;
            }
            break;
        }

        case MENU_VOLUME:
        {
            DrawText(x, y, 0xFFFFFFFF, "Volume");
            y += 40;

            s16 barW = MENU_PANEL_W - 40;
            GRRLIB_Rectangle(x, y, barW, 18, 0xFFFFFFFF, false);
            s16 fillW = (s16)(barW * settings->volume / 100);
            if (fillW > 0)
                GRRLIB_Rectangle(x + 1, y + 1, fillW - 2, 16, 0xFFCC00FF, true);
            y += 30;

            char buf[32];
            snprintf(buf, sizeof(buf), "%u%%", (unsigned)settings->volume);
            DrawText(x, y, 0xFFFFFFFF, buf);
            y += 34;
            DrawText(x, y, 0xAAAAAAFF, "LEFT / RIGHT to adjust, A/B to go back");

            /* No audio assets yet -- this stores the level for when sound
             * effects and music are added (see README "Extending the clone"). */
            break;
        }

        case MENU_CONTROLS:
        {
            DrawText(x, y, 0xFFFFFFFF, "Controls");
            y += 34;
            for (int i = 0; i < 6; ++i)
            {
                bool sel = (i == menu->cursorIndex);
                if (sel)
                    GRRLIB_Rectangle(x - 8, y - 4, MENU_PANEL_W - 24, MENU_ITEM_H, 0x4444AAFF, true);

                char line[48];
                snprintf(line, sizeof(line), "%-18s %s", kControlLabels[i], Settings_ButtonName(BindingValue(settings, i)));
                DrawText(x, y, sel ? 0xFFFF00FF : 0xFFFFFFFF, line);
                y += MENU_ITEM_H;
            }
            bool backSel = (menu->cursorIndex == CONTROLS_ITEM_COUNT - 1);
            if (backSel)
                GRRLIB_Rectangle(x - 8, y - 4, MENU_PANEL_W - 24, MENU_ITEM_H, 0x4444AAFF, true);
            DrawText(x, y, backSel ? 0xFFFF00FF : 0xFFFFFFFF, "Back");
            break;
        }

        case MENU_REBIND_WAIT:
        {
            DrawText(x, y, 0xFFFFFFFF, "Press a new button for:");
            y += 34;
            DrawText(x, y, 0xFFFF00FF, kControlLabels[menu->rebindActionIndex]);
            y += 40;
            DrawText(x, y, 0xAAAAAAFF, "(HOME cannot be reassigned)");
            break;
        }

        default:
            break;
    }
}
