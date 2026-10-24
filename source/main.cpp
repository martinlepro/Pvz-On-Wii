#include "game.h"
#include "input.h"
#include "menu.h"
#include "render.h"
#include "settings.h"
#include "audio.h"
#include "level.h"
#include "survival.h"
#include "minigame.h"
#include "save.h"

#include <gccore.h>
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include <cstdio>
#include <unistd.h>
#include <dirent.h>
#include <wiiuse/wpad.h>
#include <fat.h>
#include <di/di.h>
#include <iso9660.h>
#include "discfst.h"

/** Scans "<root>apps/" subfolders for the first one that contains an
 *  "assets" folder, and chdir()s there if found. Last-resort fallback for
 *  people who installed the app under a different folder name than
 *  "pvz_wii" (e.g. they kept the zip's own folder name), or for
 *  loaders/emulators that don't pass a usable argv[0] at all. Returns true
 *  if it found and switched to one. */
static bool TryFindAssetsUnderAppsDir(const char* root)
{
    char appsPath[64];
    snprintf(appsPath, sizeof(appsPath), "%sapps", root);

    DIR* apps = opendir(appsPath);
    if (!apps)
        return false;

    struct dirent* entry;
    bool found = false;
    while (!found && (entry = readdir(apps)) != NULL)
    {
        if (entry->d_name[0] == '.')
            continue;

        char candidate[300];
        snprintf(candidate, sizeof(candidate), "%s/%s", appsPath, entry->d_name);

        char assetsProbe[320];
        snprintf(assetsProbe, sizeof(assetsProbe), "%s/assets", candidate);
        DIR* assetsDir = opendir(assetsProbe);
        if (assetsDir)
        {
            closedir(assetsDir);
            chdir(candidate);
            found = true;
        }
    }
    closedir(apps);
    return found;
}

/**
 * When launched from the Homebrew Channel, argv[0] is the full path to
 * boot.dol (e.g. "sd:/apps/pvz_wii/boot.dol" or "usb:/apps/pvz_wii/boot.dol")
 * but the process's current working directory is NOT automatically set to
 * that folder. Every asset load in this codebase uses a plain relative path
 * ("assets/..."), so without this fix they silently fail depending on how
 * the app happens to be launched: no title background, no buttons, no seed
 * icons, no music -- just the flat green fallback rectangle behind an empty
 * title screen (the "black screen with a green square" with no visible
 * text or buttons).
 *
 * Disc builds (ISO/WBFS) do NOT get this for free: unlike sd:/ and usb:/,
 * which fatInit() mounts unconditionally, nothing mounts the optical disc
 * itself as a readable filesystem.
 *
 * Two things are tried, in order:
 *  1. ISO9660_Mount() -- only works for a disc formatted as plain ISO9660
 *     (a normal data CD/DVD layout, "CD001" signature at sector 16). The
 *     .iso/.wbfs this project actually builds via WIT (see
 *     scripts/build_iso.sh and `make iso`) is NOT that: WIT assembles a
 *     genuine Wii-format disc image with its own boot.bin/apploader/FST
 *     layout, which has no ISO9660 volume descriptor at all -- so on
 *     those images this reliably fails and is a no-op. Kept in case this
 *     code ever runs against a genuinely ISO9660-formatted disc.
 *  2. DiscFST_Mount() (source/discfst.cpp) -- reads that genuine Wii FST
 *     directly via DI_OpenPartition()/DI_Read(), which asks IOS to
 *     derive the partition's title key and hand back decrypted data (no
 *     key material is ever touched or embedded by this project's own
 *     code). This is genuinely experimental: libogc's own docs say
 *     DI_OpenPartition "will only work on original discs", and a
 *     homebrew-built disc's ticket is necessarily fake-signed, so
 *     whether IOS's verification lets it through depends on the
 *     IOS/loader environment. See discfst.cpp's header comment for the
 *     full story, including the two earlier approaches that were tried
 *     and ruled out first. Either way this fails safely (DiscFST_Mount()
 *     returns false with a specific, inspectable reason -- see
 *     DiscFST_GetStatus()), so disc builds still need assets/ reachable
 *     via SD or USB under apps/<anything>/ as the reliable fallback,
 *     exactly like the Homebrew Channel path.
 */
static void FixWorkingDirectory(int argc, char** argv)
{
    if (argc > 0 && argv && argv[0] && argv[0][0] != '\0')
    {
        char dir[512];
        strncpy(dir, argv[0], sizeof(dir) - 1);
        dir[sizeof(dir) - 1] = '\0';

        char* lastSlash = strrchr(dir, '/');
        if (lastSlash)
        {
            *lastSlash = '\0';
            chdir(dir);
        }
    }

    DIR* probe = opendir("assets");
    if (probe)
    {
        closedir(probe);
        return;
    }

    /* argv wasn't usable (or wasn't passed at all) -- fall back to the
     * documented install locations from the README. */
    static const char* kFallbackDirs[] = { "sd:/apps/pvz_wii", "usb:/apps/pvz_wii" };
    for (size_t i = 0; i < sizeof(kFallbackDirs) / sizeof(kFallbackDirs[0]); ++i)
    {
        chdir(kFallbackDirs[i]);
        probe = opendir("assets");
        if (probe)
        {
            closedir(probe);
            return;
        }
    }

    /* Still nothing -- the install folder isn't named "pvz_wii", or argv
     * pointed somewhere libfat can't resolve (some loaders/emulators pass a
     * host-filesystem path here instead of an "sd:/..." one). Scan both
     * devices' apps/ folders for any subfolder that has an assets/ inside,
     * whatever it's actually named. */
    if (TryFindAssetsUnderAppsDir("sd:/"))
        return;
    if (TryFindAssetsUnderAppsDir("usb:/"))
        return;

    /* Disc build (ISO/WBFS): none of the above applies because there is no
     * SD/USB install folder at all. First try the ISO9660 mount -- this
     * only succeeds for a disc formatted as plain ISO9660, which is NOT
     * what WIT builds here (see the big comment above FixWorkingDirectory),
     * so in practice this specific attempt is expected to fail on this
     * project's own discs. It's still harmless to try first in case this
     * code ever runs against a genuinely ISO9660-formatted disc built by
     * some other means. */
    if (ISO9660_Mount("dvd", &__io_wiidvd))
    {
        chdir("dvd:/");
        probe = opendir("assets");
        if (probe)
        {
            closedir(probe);
            return;
        }
    }

    /* Experimental: reads the disc's own native Wii FST directly via
     * DI_OpenPartition()/DI_Read() (see source/discfst.cpp for the full
     * explanation, including why this may or may not actually succeed
     * against a homebrew disc). Builds normally now (no --enc override),
     * so this either works against a real, correctly-encrypted partition
     * or fails cleanly with a specific, inspectable reason -- see
     * DiscFST_GetStatus()/DiscFST_GetOpenPartitionError(), shown as an
     * on-screen debug line next to the missing-texture counter
     * (render.cpp) so this can be diagnosed without a debugger attached. */
    if (DiscFST_Mount())
    {
        chdir("wiidisc:/");
        probe = opendir("assets");
        if (probe)
        {
            closedir(probe);
            return;
        }
    }

    /* Still nothing: leave the working directory as the loader set it. */
}

/* modes.cpp: Puzzle menu + Vasebreaker */
extern "C" {
bool Modes_Active(void);
void Modes_Update(const InputState* in, unsigned wpadDown);
void Modes_Render(const InputState* in);
}

int main(int argc, char** argv)
{
    WPAD_Init();

    fatInit(32, true);

    FixWorkingDirectory(argc, argv);

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

    /* Win / loss bookkeeping (each fires once per level, on the frame the state changes). */
    GameState prevState = game.state;
    bool      winPending = false;      /* won with no reward screen: short pause, then on */
    int       winTimer   = 0;

    while (1)
    {
        Input_Update(&input, &settings);

        /* Hand the pointer to the menu (hover/click on its on-screen buttons). */
        menu.pointerValid = input.cursorValid;
        menu.pointerX     = input.cursor.x;
        menu.pointerY     = input.cursor.y;
        menu.animFrame++;

        if (WPAD_ButtonsDown(0) & WPAD_BUTTON_HOME)
        {
            if (!Menu_IsTitle(&menu))
            {
                bool wasOpen = Menu_IsOpen(&menu);
                Menu_Toggle(&menu, &settings);
                if (!wasOpen)
                    Game_CancelHold(&game);
            }
        }

        if (Modes_Active())
        {
            /* Puzzle menu / Vasebreaker own the whole screen and all input */
            Modes_Update(&input, WPAD_ButtonsDown(0));
        }
        else if (Menu_IsTitle(&menu))
        {
            if (input.menuTogglePressed)
            {
                /* Quick start: start Adventure immediately with Peashooter */
                Level_Start(&game, (u8)Save_GetAdventureLevel()); /* continue the saved Adventure level; auto-fills the seed bar and starts */
                menu.screen = MENU_CLOSED;
            }
            else
            {
                bool startedLevel = Menu_HandleInput(&menu, &settings, &game);
                (void)startedLevel;
            }
            if (menu.quitRequested)
            {
                Settings_Save(&settings);
                Save_OnExit();             /* exiting: flush progress + coins */
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

                const bool adventure = !Survival_IsSurvival(game.currentLevelIndex) &&
                                       !Minigame_IsMinigame(game.currentLevelIndex);

                if (game.state == GAME_STATE_LEVEL_WON && prevState != GAME_STATE_LEVEL_WON)
                {
                    /* Level completed: SAVE (progress moves on), then show the plant / item
                     * reward screen if the level has one ([Next Level] + [Title Screen]).
                     * There is no separate "Level Finish" screen any more; without a reward
                     * the win just plays out for 3 s and the next level begins. */
                    Save_OnLevelWon(game.currentLevelIndex, adventure);
                    winPending = !Menu_ShowWinScreens(&menu, &game);
                    winTimer   = LEVEL_INTERMISSION_FRAMES;
                }
                else if (game.state == GAME_STATE_GAME_OVER && prevState != GAME_STATE_GAME_OVER)
                {
                    Save_OnLevelLost(game.currentLevelIndex, adventure);   /* level lost: SAVE */
                    menu.screen = MENU_DEFEAT;
                    menu.cursorIndex = 0;
                }

                if (winPending && game.state == GAME_STATE_LEVEL_WON && !Menu_IsOpen(&menu) && --winTimer <= 0)
                {
                    winPending = false;
                    Menu_AdvanceAfterWin(&menu, &settings, &game);
                }
                if (game.state != GAME_STATE_LEVEL_WON)
                    winPending = false;
                prevState = game.state;
            }
        }

        Audio_Update();

        if (Modes_Active())
        {
            Modes_Render(&input);
            Render_DrawReticle(&input);
        }
        else
        {
            Render_Frame(&game, &input);
            Menu_Render(&menu, &settings);
            if (menu.screen == MENU_NEW_PLANT)
                Render_DrawReticle(&input); /* the full-screen art would otherwise cover the pointer */
        }
        Render_Present();
    }

    Menu_Cleanup(&menu);
    Audio_Shutdown();
    Render_Shutdown();
    return 0;
}
