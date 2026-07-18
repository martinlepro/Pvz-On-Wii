#include "game.h"
#include "input.h"
#include "menu.h"
#include "render.h"
#include "settings.h"
#include "audio.h"
#include "level.h"
#include "survival.h"

#include <gccore.h>
#include <stdlib.h>
#include <wiiuse/wpad.h>
#include <fat.h>

int main(int argc, char** argv)
{
    (void)argc;
    (void)argv;

    WPAD_Init();

    fatInit();

    if (!Render_Init())
        return 1;

    Audio_Init();

    Settings settings;
    Settings_Load(&settings);

    Audio_SetMasterVolume((u8)((u16)settings.volume * 255 / 100));
    Locale_SetLanguage((Language)settings.language);

    InputState input;
    Input_Init(&input);

    GameContext game;
    Game_Init(&game);

    MenuContext menu;
    Menu_Init(&menu);
    Menu_ShowTitle(&menu, &settings);

    while (1)
    {
        Input_Update(&input, &settings);

        if (WPAD_ButtonsDown(0) & WPAD_BUTTON_HOME)
        {
            Settings_Save(&settings);
            break;
        }

        if (Menu_IsTitle(&menu))
        {
            bool startedLevel = Menu_HandleInput(&menu, &settings, &game);
            (void)startedLevel;
            if (menu.quitRequested)
            {
                Settings_Save(&settings);
                break;
            }
        }
        else
        {
            if (input.menuTogglePressed && game.state != GAME_STATE_SELECTING &&
                !(game.state == GAME_STATE_LEVEL_WON || game.state == GAME_STATE_GAME_OVER))
            {
                bool wasOpen = Menu_IsOpen(&menu);
                Menu_Toggle(&menu, &settings);
                if (!wasOpen)
                    Game_CancelHold(&game);
            }

            if (Menu_IsOpen(&menu))
                Menu_HandleInput(&menu, &settings, &game);
            else
            {
                Game_HandleInput(&game, &input);
                Game_Update(&game, &input);

                if (game.state == GAME_STATE_LEVEL_WON)
                {
                    menu.screen = MENU_VICTORY;
                    menu.cursorIndex = 0;
                    menu.canNextLevel = game.currentLevelIndex < (u8)(TOTAL_LEVELS - 1) &&
                                        !Survival_IsSurvival(game.currentLevelIndex);
                }
                else if (game.state == GAME_STATE_GAME_OVER)
                {
                    menu.screen = MENU_DEFEAT;
                    menu.cursorIndex = 0;
                }
            }
        }

        Audio_Update();

        Render_Frame(&game, &input);
        Menu_Render(&menu, &settings);
        Render_Present();
    }

    Audio_Shutdown();
    Render_Shutdown();
    return 0;
}
