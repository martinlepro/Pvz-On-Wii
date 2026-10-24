/**
 * save.cpp - Progress save system (see save.h).
 */
#include "save.h"
#include "constants.h"

#include <stdio.h>
#include <string.h>
#include <sys/stat.h>

#ifndef SETTINGS_DIR
#define SETTINGS_DIR "sd:/apps/pvz_wii"
#endif
#define SAVE_PATH SETTINGS_DIR "/save.cfg"

extern "C" void Coins_Flush(void);   /* hud.cpp: writes the coin total if it changed */

static int  s_advLevel  = 0;       /* level the Adventure button starts          */
static int  s_advDone   = 0;       /* whole Adventure cleared                    */
static bool s_loaded    = false;
static bool s_inLevel   = false;   /* a level was started and not yet saved/left */
static bool s_dirty     = false;   /* progress changed since the last write      */

static int ClampLevel(int v)
{
    if (v < 0) return 0;
    if (v > TOTAL_LEVELS - 1) return TOTAL_LEVELS - 1;
    return v;
}

static void Load(void)
{
    if (s_loaded) return;
    s_loaded = true;
    FILE* f = fopen(SAVE_PATH, "r");
    if (!f) return;
    char line[64];
    while (fgets(line, sizeof(line), f))
    {
        int v = 0;
        if (sscanf(line, "ADV_LEVEL=%d", &v) == 1) s_advLevel = ClampLevel(v);
        else if (sscanf(line, "ADV_DONE=%d", &v) == 1) s_advDone = v ? 1 : 0;
    }
    fclose(f);
}

static void Write(void)
{
    Coins_Flush();                       /* coins picked up in levels */
    mkdir(SETTINGS_DIR, 0777);
    FILE* f = fopen(SAVE_PATH, "w");
    if (f)
    {
        fprintf(f, "VERSION=1\n");
        fprintf(f, "ADV_LEVEL=%d\n", s_advLevel);
        fprintf(f, "ADV_DONE=%d\n", s_advDone);
        fclose(f);
    }
    /* no SD card: the progress simply stays in memory for this session */
    s_dirty = false;
}

extern "C" int Save_GetAdventureLevel(void)
{
    Load();
    return ClampLevel(s_advLevel);
}

extern "C" int Save_AdventureCompleted(void)
{
    Load();
    return s_advDone;
}

extern "C" void Save_NoteLevelStarted(unsigned char levelIndex, int isAdventure)
{
    Load();
    s_inLevel = true;
    if (isAdventure && (int)levelIndex > s_advLevel)
    {
        s_advLevel = ClampLevel((int)levelIndex);
        s_dirty = true;
    }
}

extern "C" void Save_OnLevelWon(unsigned char levelIndex, int isAdventure)
{
    Load();
    if (isAdventure)
    {
        if ((int)levelIndex >= TOTAL_LEVELS - 1)
        {
            s_advLevel = TOTAL_LEVELS - 1;   /* stays on 5-10 */
            s_advDone  = 1;
        }
        else if ((int)levelIndex + 1 > s_advLevel)
            s_advLevel = (int)levelIndex + 1;
    }
    s_inLevel = false;
    Write();
}

extern "C" void Save_OnLevelLost(unsigned char levelIndex, int isAdventure)
{
    Load();
    if (isAdventure && (int)levelIndex > s_advLevel)
        s_advLevel = ClampLevel((int)levelIndex);   /* retry from the same level */
    s_inLevel = false;
    Write();
}

extern "C" void Save_OnExit(void)
{
    Load();
    if (s_inLevel || s_dirty)
    {
        s_inLevel = false;
        Write();
    }
    else
        Coins_Flush();
}
