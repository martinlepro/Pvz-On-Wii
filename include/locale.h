#ifndef PVZ_LOCALE_H
#define PVZ_LOCALE_H

#ifdef __cplusplus
extern "C" {
#endif

#include "settings.h"
#include <gctypes.h>

typedef enum {
    LANG_ENGLISH = 0,
    LANG_FRENCH,
    LANG_GERMAN,
    LANG_SPANISH,
    LANG_ITALIAN,
    LANG_DUTCH,
    LANG_COUNT
} Language;

/** Detect Wii system language via SC. Returns LANG_ENGLISH as fallback. */
Language Locale_DetectSystemLanguage(void);

/** Set active language for UI. */
void Locale_SetLanguage(Language lang);

/** Get active language. */
Language Locale_GetLanguage(void);

/** Human-readable name of a language, in that language itself. */
const char* Locale_LanguageName(Language lang);

/** Number of supported languages. */
int Locale_LanguageCount(void);

/*
 * String IDs for every user-facing string in the game.
 * Translations are stored in locale.c.
 */
typedef enum {
    /* Menu principal */
    STR_TITLE,
    STR_ADVENTURE,
    STR_MINIGAMES,
    STR_OPTIONS,
    STR_EXIT,

    /* Options */
    STR_LANGUAGE,
    STR_VOLUME,
    STR_CONTROLS,
    STR_BACK,

    /* Pause menu */
    STR_RESUME,
    STR_RESTART_LEVEL,
    STR_CLOSE,

    /* Game HUD */
    STR_SUN,
    STR_WAVE,
    STR_ZOMBIES_INCOMING,
    STR_CHOOSE_PLANTS,
    STR_SELECTED,
    STR_SELECT_DESELECT,
    STR_BEGIN,

    /* Level end */
    STR_LEVEL_COMPLETE,
    STR_GAME_OVER,
    STR_ZOMBIES_GOT_IN,

    /* Minigame names */
    STR_MG_BIG_TROUBLE,
    STR_MG_NIMBLE_QUICK,
    STR_MG_COLUMN_LIKE,
    STR_MG_INVISI_GHOUL,
    STR_MG_LAST_STAND,
    STR_MG_ZOMBOTANY,
    STR_MG_WALLNUT_BOWLING,
    STR_MG_SLOT_MACHINE,
    STR_MG_RAINING_SEEDS,
    STR_MG_BEGHOULED,
    STR_MG_SEEING_STARS,
    STR_MG_ZOMBIQUARIUM,
    STR_MG_BEGHOULED_TWIST,
    STR_MG_PORTAL_COMBAT,
    STR_MG_BOBSLED,
    STR_MG_WHACK_ZOMBIE,
    STR_MG_ZOMBOTANY_2,
    STR_MG_WALLNUT_BOWLING_2,
    STR_MG_POGO_PARTY,
    STR_MG_ZOMBOSS_REVENGE,

    /* Survival menu entry */
    STR_SURVIVAL,

    /* Survival level names */
    STR_SURVIVAL_DAY_EASY,
    STR_SURVIVAL_DAY_HARD,
    STR_SURVIVAL_NIGHT_EASY,
    STR_SURVIVAL_NIGHT_HARD,
    STR_SURVIVAL_POOL_EASY,
    STR_SURVIVAL_POOL_HARD,
    STR_SURVIVAL_FOG_EASY,
    STR_SURVIVAL_FOG_HARD,
    STR_SURVIVAL_ROOF_EASY,
    STR_SURVIVAL_ROOF_HARD,
    STR_SURVIVAL_ENDLESS,

    STR_COUNT
} StringID;

/** Return the localized string for the given ID in the active language. */
const char* Locale_Str(StringID id);

#ifdef __cplusplus
}
#endif

#endif
