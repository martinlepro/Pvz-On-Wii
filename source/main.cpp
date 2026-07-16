/**
 * main.cpp - Entry point: wires input, game simulation, the options menu,
 * and rendering together.
 *
 * Init order matters: Render_Init() brings up video *and* the filesystem
 * (GRRLIB_Init() mounts the SD/DVD devices it needs internally), so it must
 * run before Settings_Load(), which reads sd:/apps/pvz_wii/settings.cfg.
 */
#include "game.h"
#include "input.h"
#include "menu.h"
#include "render.h"
#include "settings.h"

#include <gccore.h>
#include <stdlib.h>
#include <wiiuse/wpad.h>

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    WPAD_Init();

    if (!Render_Init())
        return 1;

    Settings settings;
    Settings_Load(&settings);

    InputState input;
    Input_Init(&input);

    GameContext game;
    Game_Init(&game);

    MenuContext menu;
    Menu_Init(&menu);

    while (1)
    {
        Input_Update(&input, &settings);

        /* HOME always exits, regardless of remapping or menu state. */
        if (WPAD_ButtonsDown(0) & WPAD_BUTTON_HOME)
        {
            Settings_Save(&settings);
            break;
        }

        /* - or + toggles the menu open/closed; opening it drops any seed
         * currently being held so a hold never lingers across a pause. */
        if (input.menuTogglePressed)
        {
            bool wasOpen = Menu_IsOpen(&menu);
            Menu_Toggle(&menu, &settings);
            if (!wasOpen)
                Game_CancelHold(&game);
        }

        if (Menu_IsOpen(&menu))
        {
            Menu_HandleInput(&menu, &settings, &game);
        }
        else
        {
            Game_HandleInput(&game, &input);
            Game_Update(&game, &input);
        }

        Render_Frame(&game, &input);
        if (Menu_IsOpen(&menu))
            Menu_Render(&menu, &settings);
        Render_Present();
    }

    Render_Shutdown();
    return 0;
}
