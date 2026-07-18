#include "menu.h"
#include "audio.h"
#include "constants.h"
#include "render.h"
#include "level.h"
#include "locale.h"
#include "survival.h"
#include "minigame.h"

#include <grrlib.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <wiiuse/wpad.h>

#define MAIN_ITEM_COUNT     5
#define CONTROLS_ITEM_COUNT 7

static const char* kControlLabels[6] = {
    "Place (primary)", "Place (secondary)",
    "Next Seed",        "Previous Seed",
    "Menu (primary)",  "Menu (secondary)"
};

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

void Menu_Init(MenuContext* menu)
{
    if (!menu) return;
    memset(menu, 0, sizeof(*menu));
    menu->screen = MENU_TITLE;
    menu->quitRequested = false;
}

bool Menu_IsOpen(const MenuContext* menu)
{
    return menu && menu->screen != MENU_CLOSED;
}

bool Menu_IsTitle(const MenuContext* menu)
{
    return menu && (menu->screen == MENU_TITLE ||
                    menu->screen == MENU_TITLE_OPTIONS ||
                    menu->screen == MENU_TITLE_LANGUAGE ||
                    menu->screen == MENU_MINIGAME_SELECT ||
                    menu->screen == MENU_SURVIVAL_SELECT);
}

static bool Menu_IsResultScreen(const MenuContext* menu)
{
    return menu && (menu->screen == MENU_VICTORY || menu->screen == MENU_DEFEAT);
}

void Menu_Toggle(MenuContext* menu, Settings* settings)
{
    if (!menu) return;

    if (menu->screen == MENU_CLOSED)
    {
        menu->screen = MENU_MAIN;
        menu->cursorIndex = 0;
    }
    else if (!Menu_IsTitle(menu))
    {
        menu->screen = MENU_CLOSED;
        Settings_Save(settings);
    }
}

void Menu_ShowTitle(MenuContext* menu, Settings* settings)
{
    if (!menu) return;
    Audio_StopBGM();
    menu->screen = MENU_TITLE;
    menu->cursorIndex = 0;
    Locale_SetLanguage((Language)settings->language);
}

bool Menu_HandleInput(MenuContext* menu, Settings* settings, GameContext* game)
{
    if (!menu || !settings) return false;

    u32 down = WPAD_ButtonsDown(0);
    bool startedLevel = false;

    if (menu->screen == MENU_REBIND_WAIT)
    {
        int count = Settings_RemappableButtonCount();
        for (int i = 0; i < count; ++i)
        {
            u32 candidate = Settings_RemappableButtonAt(i);
            if (down & candidate)
            {
                u32* slot = BindingSlot(settings, menu->rebindActionIndex);
                if (slot) *slot = candidate;
                menu->screen = MENU_CONTROLS;
                return false;
            }
        }
        return false;
    }

    if (down & WPAD_BUTTON_UP)
        menu->cursorIndex--;
    if (down & WPAD_BUTTON_DOWN)
        menu->cursorIndex++;

    switch (menu->screen)
    {
        case MENU_TITLE:
        {
            int itemCount = 5; /* Adventure, Minigames, Survival, Options, Exit */
            if (menu->cursorIndex < 0) menu->cursorIndex = itemCount - 1;
            if (menu->cursorIndex >= itemCount) menu->cursorIndex = 0;

            if (down & WPAD_BUTTON_A)
            {
                switch (menu->cursorIndex)
                {
                    case 0: /* Adventure */
                        Level_Start(game, 0);
                        Locale_SetLanguage((Language)settings->language);
                        menu->screen = MENU_CLOSED;
                        startedLevel = true;
                        break;
                    case 1: /* Minigames */
                        menu->screen = MENU_MINIGAME_SELECT;
                        menu->cursorIndex = 0;
                        break;
                    case 2: /* Survival */
                        menu->screen = MENU_SURVIVAL_SELECT;
                        menu->cursorIndex = 0;
                        break;
                    case 3: /* Options */
                        menu->screen = MENU_TITLE_OPTIONS;
                        menu->cursorIndex = 0;
                        break;
                    case 4: /* Exit */
                        menu->quitRequested = true;
                        break;
                }
            }
            break;
        }

        case MENU_TITLE_OPTIONS:
        {
            int itemCount = 3; /* Language, Controls, Back */
            if (menu->cursorIndex < 0) menu->cursorIndex = itemCount - 1;
            if (menu->cursorIndex >= itemCount) menu->cursorIndex = 0;

            if (down & WPAD_BUTTON_A)
            {
                switch (menu->cursorIndex)
                {
                    case 0: /* Language */
                        menu->screen = MENU_TITLE_LANGUAGE;
                        menu->cursorIndex = 0;
                        break;
                    case 1: /* Controls */
                        menu->screen = MENU_CONTROLS;
                        menu->cursorIndex = 0;
                        break;
                    case 2: /* Back */
                        menu->screen = MENU_TITLE;
                        menu->cursorIndex = 2;
                        break;
                }
            }
            else if (down & WPAD_BUTTON_B)
            {
                menu->screen = MENU_TITLE;
                menu->cursorIndex = 2;
            }
            break;
        }

        case MENU_TITLE_LANGUAGE:
        {
            int langCount = Locale_LanguageCount();
            if (menu->cursorIndex < 0) menu->cursorIndex = langCount - 1;
            if (menu->cursorIndex >= langCount) menu->cursorIndex = 0;

            if (down & WPAD_BUTTON_A)
            {
                settings->language = (u8)menu->cursorIndex;
                Locale_SetLanguage((Language)settings->language);
                menu->screen = MENU_TITLE_OPTIONS;
                menu->cursorIndex = 0;
            }
            else if (down & WPAD_BUTTON_B)
            {
                menu->screen = MENU_TITLE_OPTIONS;
                menu->cursorIndex = 0;
            }
            break;
        }

        case MENU_MINIGAME_SELECT:
        {
            int itemCount = 6; /* 5 minigames + Back */
            if (menu->cursorIndex < 0) menu->cursorIndex = itemCount - 1;
            if (menu->cursorIndex >= itemCount) menu->cursorIndex = 0;

            if (down & WPAD_BUTTON_A)
            {
                if (menu->cursorIndex == itemCount - 1)
                {
                    menu->screen = MENU_TITLE;
                    menu->cursorIndex = 1;
                }
                else
                {
                    u8 levelIdx = (u8)(MINIGAME_OFFSET + menu->cursorIndex);
                    Level_Start(game, levelIdx);
                    Locale_SetLanguage((Language)settings->language);
                    menu->screen = MENU_CLOSED;
                    startedLevel = true;
                }
            }
            else if (down & WPAD_BUTTON_B)
            {
                menu->screen = MENU_TITLE;
                menu->cursorIndex = 1;
            }
            break;
        }

        case MENU_SURVIVAL_SELECT:
        {
            int itemCount = SURVIVAL_COUNT + 1; /* 11 survival + Back */
            if (menu->cursorIndex < 0) menu->cursorIndex = itemCount - 1;
            if (menu->cursorIndex >= itemCount) menu->cursorIndex = 0;

            if (down & WPAD_BUTTON_A)
            {
                if (menu->cursorIndex == itemCount - 1)
                {
                    menu->screen = MENU_TITLE;
                    menu->cursorIndex = 2;
                }
                else
                {
                    u8 levelIdx = (u8)(SURVIVAL_OFFSET + menu->cursorIndex);
                    Level_Start(game, levelIdx);
                    Locale_SetLanguage((Language)settings->language);
                    menu->screen = MENU_CLOSED;
                    startedLevel = true;
                }
            }
            else if (down & WPAD_BUTTON_B)
            {
                menu->screen = MENU_TITLE;
                menu->cursorIndex = 2;
            }
            break;
        }

        case MENU_MAIN:
        {
            int itemCount = MAIN_ITEM_COUNT;
            if (menu->cursorIndex < 0) menu->cursorIndex = itemCount - 1;
            if (menu->cursorIndex >= itemCount) menu->cursorIndex = 0;

            if (down & WPAD_BUTTON_A)
            {
                switch (menu->cursorIndex)
                {
                    case 0: menu->screen = MENU_VOLUME; break;
                    case 1: menu->screen = MENU_CONTROLS; menu->cursorIndex = 0; break;
                    case 2: if (game) Level_Start(game, game->currentLevelIndex); break;
                    case 3: /* Main Menu (back to title) */
                        Menu_ShowTitle(menu, settings);
                        break;
                    case 4: Menu_Toggle(menu, settings); break;
                }
            }
            else if (down & WPAD_BUTTON_B)
            {
                Menu_Toggle(menu, settings);
            }
            break;
        }

        case MENU_VOLUME:
        {
            if (down & WPAD_BUTTON_LEFT)
            {
                settings->volume = (settings->volume >= 5) ? (u8)(settings->volume - 5) : 0;
                Audio_SetMasterVolume((u8)((u16)settings->volume * 255 / 100));
            }
            if (down & WPAD_BUTTON_RIGHT)
            {
                settings->volume = (settings->volume <= 95) ? (u8)(settings->volume + 5) : 100;
                Audio_SetMasterVolume((u8)((u16)settings->volume * 255 / 100));
            }
            if (down & (WPAD_BUTTON_A | WPAD_BUTTON_B))
            {
                menu->screen = MENU_MAIN;
                menu->cursorIndex = 0;
            }
            break;
        }

        case MENU_CONTROLS:
        {
            int itemCount = CONTROLS_ITEM_COUNT;
            if (menu->cursorIndex < 0) menu->cursorIndex = itemCount - 1;
            if (menu->cursorIndex >= itemCount) menu->cursorIndex = 0;

            if (down & WPAD_BUTTON_A)
            {
                if (menu->cursorIndex == CONTROLS_ITEM_COUNT - 1)
                {
                    /* Check if we came from title or pause */
                    if (Menu_IsTitle(menu))
                    {
                        menu->screen = MENU_TITLE_OPTIONS;
                        menu->cursorIndex = 1;
                    }
                    else
                    {
                        menu->screen = MENU_MAIN;
                        menu->cursorIndex = 1;
                    }
                }
                else
                {
                    menu->rebindActionIndex = menu->cursorIndex;
                    menu->screen = MENU_REBIND_WAIT;
                }
            }
            else if (down & WPAD_BUTTON_B)
            {
                if (Menu_IsTitle(menu))
                {
                    menu->screen = MENU_TITLE_OPTIONS;
                    menu->cursorIndex = 1;
                }
                else
                {
                    menu->screen = MENU_MAIN;
                    menu->cursorIndex = 1;
                }
            }
            break;
        }

        case MENU_VICTORY:
        {
            if (!game) { menu->screen = MENU_CLOSED; break; }
            bool canNext = menu->canNextLevel;
            int itemCount = canNext ? 3 : 2;
            if (menu->cursorIndex < 0) menu->cursorIndex = itemCount - 1;
            if (menu->cursorIndex >= itemCount) menu->cursorIndex = 0;

            if (down & WPAD_BUTTON_A)
            {
                if (menu->cursorIndex == 0 && canNext)
                {
                    menu->screen = MENU_CLOSED;
                    game->state = GAME_STATE_SELECTING;
                    Level_Start(game, (u8)(game->currentLevelIndex + 1));
                }
                else if ((menu->cursorIndex == 0 && !canNext) ||
                         (menu->cursorIndex == 1 && canNext))
                {
                    menu->screen = MENU_CLOSED;
                    Level_Start(game, game->currentLevelIndex);
                }
                else
                {
                    Game_Reset(game);
                    Menu_ShowTitle(menu, settings);
                }
            }
            else if (down & WPAD_BUTTON_B)
            {
                menu->screen = MENU_CLOSED;
                Level_Start(game, game->currentLevelIndex);
            }
            break;
        }

        case MENU_DEFEAT:
        {
            if (!game) { menu->screen = MENU_CLOSED; break; }
            int itemCount = 2;
            if (menu->cursorIndex < 0) menu->cursorIndex = itemCount - 1;
            if (menu->cursorIndex >= itemCount) menu->cursorIndex = 0;

            if (down & WPAD_BUTTON_A)
            {
                if (menu->cursorIndex == 0)
                {
                    menu->screen = MENU_CLOSED;
                    Level_Start(game, game->currentLevelIndex);
                }
                else
                {
                    Game_Reset(game);
                    Menu_ShowTitle(menu, settings);
                }
            }
            else if (down & WPAD_BUTTON_B)
            {
                menu->screen = MENU_CLOSED;
                Level_Start(game, game->currentLevelIndex);
            }
            break;
        }

        default:
            break;
    }

    return startedLevel;
}

static void DrawText(s16 x, s16 y, u32 color, const char* text)
{
    GRRLIB_ttfFont* font = Render_GetFont();
    if (font)
        GRRLIB_PrintfTTF(x, y, font, text, 16, color);
}

static void DrawTextLarge(s16 x, s16 y, u32 color, const char* text)
{
    GRRLIB_ttfFont* font = Render_GetFont();
    if (font)
        GRRLIB_PrintfTTF(x, y, font, text, 28, color);
}

void Menu_Render(const MenuContext* menu, const Settings* settings)
{
    if (!menu) return;

    if (menu->screen == MENU_TITLE || menu->screen == MENU_TITLE_OPTIONS ||
        menu->screen == MENU_TITLE_LANGUAGE || menu->screen == MENU_MINIGAME_SELECT ||
        menu->screen == MENU_SURVIVAL_SELECT)
    {
        /* Full screen title background */
        u32 bgColor = 0x1B3B1BFF;
        GRRLIB_Rectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, bgColor, true);

        /* Decorative grass lines */
        for (int i = 0; i < 20; i++)
        {
            s16 gx = (s16)(rand() % SCREEN_WIDTH);
            s16 gy = (s16)(SCREEN_HEIGHT - 40 - rand() % 60);
            GRRLIB_Rectangle(gx, gy, 3, 12, 0x2D5A2DEE, true);
        }

        s16 cx = SCREEN_WIDTH / 2 - 200;

        switch (menu->screen)
        {
            case MENU_TITLE:
            {
                DrawTextLarge(cx + 10, 40, 0xCCFF66FF, Locale_Str(STR_TITLE));

                /* Lawn stripe decoration */
                GRRLIB_Rectangle(0, 120, SCREEN_WIDTH, 2, 0x88AA44CC, true);

                s16 y = 160;
                int itemCount = 5;
                const char* items[5] = {
                    Locale_Str(STR_ADVENTURE),
                    Locale_Str(STR_MINIGAMES),
                    Locale_Str(STR_SURVIVAL),
                    Locale_Str(STR_OPTIONS),
                    Locale_Str(STR_EXIT)
                };

                for (int i = 0; i < itemCount; ++i)
                {
                    bool sel = (i == menu->cursorIndex);
                    s16 bx = cx + 10;
                    s16 by = y - 4;
                    if (sel)
                    {
                        GRRLIB_Rectangle(bx - 12, by, 290, MENU_ITEM_H, 0x4444AAFF, true);
                    }
                    DrawText(bx + 8, y, sel ? 0xFFFF00FF : 0xFFFFFFFF, items[i]);
                    y += MENU_ITEM_H + 8;
                }
                break;
            }

            case MENU_TITLE_OPTIONS:
            {
                DrawTextLarge(cx + 10, 60, 0xCCFF66FF, Locale_Str(STR_OPTIONS));
                s16 y = 130;
                int itemCount = 3;
                const char* items[3] = {
                    Locale_Str(STR_LANGUAGE),
                    "Controls",
                    Locale_Str(STR_BACK)
                };

                for (int i = 0; i < itemCount; ++i)
                {
                    bool sel = (i == menu->cursorIndex);
                    s16 bx = cx + 10;
                    s16 by = y - 4;
                    if (sel)
                        GRRLIB_Rectangle(bx - 12, by, 290, MENU_ITEM_H, 0x4444AAFF, true);

                    if (i == 0)
                    {
                        char buf[64];
                        snprintf(buf, sizeof(buf), "%s: %s", items[i],
                                 Locale_LanguageName((Language)settings->language));
                        DrawText(bx + 8, y, sel ? 0xFFFF00FF : 0xFFFFFFFF, buf);
                    }
                    else
                    {
                        DrawText(bx + 8, y, sel ? 0xFFFF00FF : 0xFFFFFFFF, items[i]);
                    }
                    y += MENU_ITEM_H + 8;
                }
                break;
            }

            case MENU_TITLE_LANGUAGE:
            {
                DrawTextLarge(cx + 10, 60, 0xCCFF66FF, Locale_Str(STR_LANGUAGE));
                s16 y = 130;
                int langCount = Locale_LanguageCount();
                for (int i = 0; i < langCount; ++i)
                {
                    bool sel = (i == menu->cursorIndex);
                    s16 bx = cx + 10;
                    s16 by = y - 4;
                    if (sel)
                        GRRLIB_Rectangle(bx - 12, by, 290, MENU_ITEM_H, 0x4444AAFF, true);

                    const char* name = Locale_LanguageName((Language)i);
                    bool current = (i == (int)settings->language);
                    char buf[64];
                    if (current)
                        snprintf(buf, sizeof(buf), "> %s <", name);
                    else
                        snprintf(buf, sizeof(buf), "  %s", name);
                    DrawText(bx + 8, y, sel ? 0xFFFF00FF : (current ? 0x88FF88FF : 0xFFFFFFFF), buf);
                    y += MENU_ITEM_H + 8;
                }
                break;
            }

            case MENU_MINIGAME_SELECT:
            {
                DrawTextLarge(cx + 10, 60, 0xCCFF66FF, Locale_Str(STR_MINIGAMES));
                s16 y = 130;
                int itemCount = 6;
                const char* items[6] = {
                    Locale_Str(STR_MG_BIG_TROUBLE),
                    Locale_Str(STR_MG_NIMBLE_QUICK),
                    Locale_Str(STR_MG_COLUMN_LIKE),
                    Locale_Str(STR_MG_INVISI_GHOUL),
                    Locale_Str(STR_MG_LAST_STAND),
                    Locale_Str(STR_BACK)
                };

                for (int i = 0; i < itemCount; ++i)
                {
                    bool sel = (i == menu->cursorIndex);
                    s16 bx = cx + 10;
                    s16 by = y - 4;
                    if (sel)
                        GRRLIB_Rectangle(bx - 12, by, 330, MENU_ITEM_H, 0x4444AAFF, true);
                    DrawText(bx + 8, y, sel ? 0xFFFF00FF : 0xFFFFFFFF, items[i]);
                    y += MENU_ITEM_H + 8;
                }
                break;
            }

            case MENU_SURVIVAL_SELECT:
            {
                DrawTextLarge(cx + 10, 60, 0xCCFF66FF, Locale_Str(STR_SURVIVAL));
                s16 y = 130;
                int itemCount = SURVIVAL_COUNT + 1;
                const char* items[12];
                items[0] = Locale_Str(STR_SURVIVAL_DAY_EASY);
                items[1] = Locale_Str(STR_SURVIVAL_DAY_HARD);
                items[2] = Locale_Str(STR_SURVIVAL_NIGHT_EASY);
                items[3] = Locale_Str(STR_SURVIVAL_NIGHT_HARD);
                items[4] = Locale_Str(STR_SURVIVAL_POOL_EASY);
                items[5] = Locale_Str(STR_SURVIVAL_POOL_HARD);
                items[6] = Locale_Str(STR_SURVIVAL_FOG_EASY);
                items[7] = Locale_Str(STR_SURVIVAL_FOG_HARD);
                items[8] = Locale_Str(STR_SURVIVAL_ROOF_EASY);
                items[9] = Locale_Str(STR_SURVIVAL_ROOF_HARD);
                items[10] = Locale_Str(STR_SURVIVAL_ENDLESS);
                items[11] = Locale_Str(STR_BACK);

                for (int i = 0; i < itemCount; ++i)
                {
                    bool sel = (i == menu->cursorIndex);
                    s16 bx = cx + 10;
                    s16 by = y - 4;
                    if (sel)
                        GRRLIB_Rectangle(bx - 12, by, 360, MENU_ITEM_H, 0x4444AAFF, true);
                    DrawText(bx + 8, y, sel ? 0xFFFF00FF : 0xFFFFFFFF, items[i]);
                    y += MENU_ITEM_H + 8;
                }
                break;
            }

            default:
                break;
        }
        return;
    }

    if (menu->screen == MENU_CLOSED) return;

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
            int itemCount = MAIN_ITEM_COUNT;
            const char* items[5] = {
                "Volume", "Controls",
                "Restart Level",
                "Main Menu",
                "Close"
            };
            for (int i = 0; i < itemCount; ++i)
            {
                bool sel = (i == menu->cursorIndex);
                if (sel)
                    GRRLIB_Rectangle(x - 8, y - 4, MENU_PANEL_W - 24, MENU_ITEM_H, 0x4444AAFF, true);
                DrawText(x, y, sel ? 0xFFFF00FF : 0xFFFFFFFF, items[i]);
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

        case MENU_VICTORY:
        {
            GRRLIB_Rectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0x00000080, true);
            DrawTextLarge(SCREEN_WIDTH / 2 - 120, SCREEN_HEIGHT / 2 - 80, 0xFFDD33FF, "LEVEL WON!");
            y = SCREEN_HEIGHT / 2 - 30;
            x = SCREEN_WIDTH / 2 - 80;
            int itemCount;
            const char* items[3];
            if (menu->canNextLevel)
            {
                itemCount = 3;
                items[0] = "Next Level";
                items[1] = "Retry";
                items[2] = "Main Menu";
            }
            else
            {
                itemCount = 2;
                items[0] = "Retry";
                items[1] = "Main Menu";
            }
            for (int i = 0; i < itemCount; i++)
            {
                bool sel = (i == menu->cursorIndex);
                if (sel)
                    GRRLIB_Rectangle(x - 8, y - 4, 180, MENU_ITEM_H, 0x448844FF, true);
                DrawText(x, y, sel ? 0xFFFF00FF : 0xFFFFFFFF, items[i]);
                y += MENU_ITEM_H + 4;
            }
            break;
        }

        case MENU_DEFEAT:
        {
            GRRLIB_Rectangle(0, 0, SCREEN_WIDTH, SCREEN_HEIGHT, 0x00000080, true);
            DrawTextLarge(SCREEN_WIDTH / 2 - 120, SCREEN_HEIGHT / 2 - 80, 0xFF4444FF, "GAME OVER");
            y = SCREEN_HEIGHT / 2 - 30;
            x = SCREEN_WIDTH / 2 - 80;
            int itemCount = 2;
            const char* items[2] = { "Retry", "Main Menu" };
            for (int i = 0; i < itemCount; i++)
            {
                bool sel = (i == menu->cursorIndex);
                if (sel)
                    GRRLIB_Rectangle(x - 8, y - 4, 180, MENU_ITEM_H, 0x884444FF, true);
                DrawText(x, y, sel ? 0xFFFF00FF : 0xFFFFFFFF, items[i]);
                y += MENU_ITEM_H + 4;
            }
            break;
        }

        default:
            break;
    }
}
