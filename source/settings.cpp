/**
 * settings.cpp - Load/save persisted volume, control bindings, and language.
 *
 * Format is a small human-readable KEY=VALUE text file (mirrors the style
 * already used by disc/meta/disc.info), so it is easy to inspect or hand-edit
 * on the SD card if needed:
 *
 *   VOLUME=80
 *   LANGUAGE=0
 *   KEY_PLACE_PRIMARY=A
 *   KEY_PLACE_SECONDARY=B
 *   KEY_MOVE_RIGHT=1
 *   KEY_MOVE_LEFT=2
 *   KEY_MENU_PRIMARY=PLUS
 *   KEY_MENU_SECONDARY=MINUS
 */
#include "settings.h"
#include "constants.h"
#include "locale.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/stat.h>
#include <wiiuse/wpad.h>

/* -------------------------------------------------------------------------- */
/* Button <-> name table                                                      */
/* -------------------------------------------------------------------------- */

typedef struct {
    const char* name;
    u32         mask;
} ButtonNameEntry;

/* HOME is included so a settings file that somehow references it still
 * parses, but it is deliberately left out of Settings_RemappableButtonAt()
 * below -- HOME must always exit the game. */
static const ButtonNameEntry s_buttonTable[] = {
    { "A",     WPAD_BUTTON_A     },
    { "B",     WPAD_BUTTON_B     },
    { "1",     WPAD_BUTTON_1     },
    { "2",     WPAD_BUTTON_2     },
    { "UP",    WPAD_BUTTON_UP    },
    { "DOWN",  WPAD_BUTTON_DOWN  },
    { "LEFT",  WPAD_BUTTON_LEFT  },
    { "RIGHT", WPAD_BUTTON_RIGHT },
    { "PLUS",  WPAD_BUTTON_PLUS  },
    { "MINUS", WPAD_BUTTON_MINUS},
    { "HOME",  WPAD_BUTTON_HOME  },
};
static const int kButtonTableCount = (int)(sizeof(s_buttonTable) / sizeof(s_buttonTable[0]));

/* Candidates offered by the Controls menu -- everything except HOME. */
static const u32 s_remappable[] = {
    WPAD_BUTTON_A, WPAD_BUTTON_B, WPAD_BUTTON_1, WPAD_BUTTON_2,
    WPAD_BUTTON_UP, WPAD_BUTTON_DOWN, WPAD_BUTTON_LEFT, WPAD_BUTTON_RIGHT,
    WPAD_BUTTON_PLUS, WPAD_BUTTON_MINUS,
};
static const int kRemappableCount = (int)(sizeof(s_remappable) / sizeof(s_remappable[0]));

const char* Settings_ButtonName(u32 mask)
{
    for (int i = 0; i < kButtonTableCount; ++i)
    {
        if (s_buttonTable[i].mask == mask)
            return s_buttonTable[i].name;
    }
    return "?";
}

u32 Settings_ButtonFromName(const char* name)
{
    if (!name)
        return 0;
    for (int i = 0; i < kButtonTableCount; ++i)
    {
        if (strcmp(s_buttonTable[i].name, name) == 0)
            return s_buttonTable[i].mask;
    }
    return 0;
}

int Settings_RemappableButtonCount(void)
{
    return kRemappableCount;
}

u32 Settings_RemappableButtonAt(int index)
{
    if (index < 0 || index >= kRemappableCount)
        return 0;
    return s_remappable[index];
}

/* -------------------------------------------------------------------------- */
/* Defaults / load / save                                                    */
/* -------------------------------------------------------------------------- */

void Settings_SetDefaults(Settings* settings)
{
    if (!settings)
        return;

    settings->volume            = 80;
    settings->language          = (u8)Locale_DetectSystemLanguage();
    settings->keyPlacePrimary   = WPAD_BUTTON_A;
    settings->keyPlaceSecondary = WPAD_BUTTON_B;
    settings->keyMoveRight      = WPAD_BUTTON_1;
    settings->keyMoveLeft       = WPAD_BUTTON_2;
    settings->keyMenuPrimary    = WPAD_BUTTON_PLUS;
    settings->keyMenuSecondary  = WPAD_BUTTON_MINUS;
}

static void TrimNewline(char* s)
{
    size_t len = strlen(s);
    while (len > 0 && (s[len - 1] == '\n' || s[len - 1] == '\r'))
    {
        s[len - 1] = '\0';
        --len;
    }
}

void Settings_Load(Settings* settings)
{
    if (!settings)
        return;

    /* Always start from a known-good default so a partial or corrupt file
     * only overrides the fields it actually contains. */
    Settings_SetDefaults(settings);

    FILE* f = fopen(SETTINGS_PATH, "r");
    if (!f)
        return; /* No SD card, or first run -- defaults stand. */

    char line[128];
    while (fgets(line, sizeof(line), f))
    {
        TrimNewline(line);
        if (line[0] == '\0' || line[0] == '#')
            continue;

        char* eq = strchr(line, '=');
        if (!eq)
            continue;
        *eq = '\0';
        const char* key = line;
        const char* val = eq + 1;

        if (strcmp(key, "VOLUME") == 0)
        {
            int v = atoi(val);
            if (v < 0)   v = 0;
            if (v > 100) v = 100;
            settings->volume = (u8)v;
        }
        else if (strcmp(key, "LANGUAGE") == 0)
        {
            int v = atoi(val);
            if (v >= 0 && v < LANG_COUNT)
                settings->language = (u8)v;
        }
        else if (strcmp(key, "KEY_PLACE_PRIMARY") == 0)
        {
            u32 m = Settings_ButtonFromName(val);
            if (m) settings->keyPlacePrimary = m;
        }
        else if (strcmp(key, "KEY_PLACE_SECONDARY") == 0)
        {
            u32 m = Settings_ButtonFromName(val);
            if (m) settings->keyPlaceSecondary = m;
        }
        else if (strcmp(key, "KEY_MOVE_RIGHT") == 0)
        {
            u32 m = Settings_ButtonFromName(val);
            if (m) settings->keyMoveRight = m;
        }
        else if (strcmp(key, "KEY_MOVE_LEFT") == 0)
        {
            u32 m = Settings_ButtonFromName(val);
            if (m) settings->keyMoveLeft = m;
        }
        else if (strcmp(key, "KEY_MENU_PRIMARY") == 0)
        {
            u32 m = Settings_ButtonFromName(val);
            if (m) settings->keyMenuPrimary = m;
        }
        else if (strcmp(key, "KEY_MENU_SECONDARY") == 0)
        {
            u32 m = Settings_ButtonFromName(val);
            if (m) settings->keyMenuSecondary = m;
        }
        /* Unknown keys are ignored so newer settings files still load on
         * older builds and vice versa. */
    }

    fclose(f);
}

bool Settings_Save(const Settings* settings)
{
    if (!settings)
        return false;

    /* Best-effort directory creation; ignore failure (already exists, or
     * no SD card present at all -- the fopen below will fail cleanly). */
    mkdir(SETTINGS_DIR, 0777);

    FILE* f = fopen(SETTINGS_PATH, "w");
    if (!f)
        return false;

    fprintf(f, "# Plants vs Zombies Wii - saved settings\n");
    fprintf(f, "VOLUME=%u\n", (unsigned)settings->volume);
    fprintf(f, "LANGUAGE=%u\n", (unsigned)settings->language);
    fprintf(f, "KEY_PLACE_PRIMARY=%s\n",   Settings_ButtonName(settings->keyPlacePrimary));
    fprintf(f, "KEY_PLACE_SECONDARY=%s\n", Settings_ButtonName(settings->keyPlaceSecondary));
    fprintf(f, "KEY_MOVE_RIGHT=%s\n",      Settings_ButtonName(settings->keyMoveRight));
    fprintf(f, "KEY_MOVE_LEFT=%s\n",       Settings_ButtonName(settings->keyMoveLeft));
    fprintf(f, "KEY_MENU_PRIMARY=%s\n",    Settings_ButtonName(settings->keyMenuPrimary));
    fprintf(f, "KEY_MENU_SECONDARY=%s\n",  Settings_ButtonName(settings->keyMenuSecondary));

    fclose(f);
    return true;
}
