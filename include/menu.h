/**
 * menu.h - Pause/options menu opened with - or + (see input.h).
 *
 * Lets the player adjust the master volume (wiring for actual audio comes
 * later -- see README) and remap the four gameplay actions away from their
 * defaults. Both are persisted via settings.h whenever the menu is closed.
 *
 * Navigation uses the Wiimote D-Pad + A/B directly (menu navigation itself
 * is not remappable -- only the four gameplay actions are).
 */
#ifndef PVZ_MENU_H
#define PVZ_MENU_H

#ifdef __cplusplus
extern "C" {
#endif

#include "game.h"
#include "settings.h"
#include "locale.h"
#include "plant.h"
#include <stdbool.h>
#include <grrlib.h>

typedef enum {
    MENU_CLOSED = 0,
    MENU_TITLE,
    MENU_TITLE_OPTIONS,
    MENU_TITLE_LANGUAGE,
    MENU_MINIGAME_SELECT,
    MENU_SURVIVAL_SELECT,
    MENU_MAIN,
    MENU_VOLUME,
    MENU_CONTROLS,
    MENU_REBIND_WAIT,
    MENU_VICTORY,
    MENU_DEFEAT
} MenuScreen;

typedef struct {
    MenuScreen screen;
    s8         cursorIndex;       /* highlighted row in the current list */
    s8         rebindActionIndex; /* which of the 6 bindings is being reassigned */
    bool       canNextLevel;      /* cached for MENU_VICTORY rendering */
    PlantType  rewardPlant;       /* PLANT_NONE if no reward, or the plant just unlocked */
    bool       quitRequested;     /* set when user picks Exit on the title screen */

    /* Title screen button textures (loaded from assets/button/) */
    GRRLIB_texImg* btnAdventure;
    GRRLIB_texImg* btnMinigame;
    GRRLIB_texImg* btnSurvival;
    GRRLIB_texImg* btnPuzzle;
} MenuContext;

void Menu_Init(MenuContext* menu);

/** True whenever the menu is showing (gameplay input should be suppressed). */
bool Menu_IsOpen(const MenuContext* menu);
bool Menu_IsTitle(const MenuContext* menu);

/** Open if closed; if open, close and persist settings to the SD card. */
void Menu_Toggle(MenuContext* menu, Settings* settings);

/** Switch to title screen state. */
void Menu_ShowTitle(MenuContext* menu, Settings* settings);

/** Handle D-Pad/A/B navigation for whichever screen is currently shown.
 *  `game` may be NULL (only needed for the "Reset Lawn" action).
 *  Returns true if the caller should start a level (Adventure or minigame). */
bool Menu_HandleInput(MenuContext* menu, Settings* settings, GameContext* game);

/** Draw the menu panel on top of the current frame (call after Render_Frame). */
void Menu_Render(const MenuContext* menu, const Settings* settings);

/** Free button textures loaded by Menu_Init. */
void Menu_Cleanup(MenuContext* menu);

#ifdef __cplusplus
}
#endif

#endif /* PVZ_MENU_H */
