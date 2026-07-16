/**
 * settings.h - Persisted user preferences: master volume and Wiimote control
 * bindings. Loaded once at startup and saved back to the SD card whenever the
 * in-game options menu is closed.
 *
 * File location: sd:/apps/pvz_wii/settings.cfg (plain KEY=VALUE text, same
 * convention as disc/meta/disc.info elsewhere in this project).
 */
#ifndef PVZ_SETTINGS_H
#define PVZ_SETTINGS_H

#ifdef __cplusplus
extern "C" {
#endif

#include <ogc/types.h>
#include <stdbool.h>

/**
 * Every field stores a WPAD_BUTTON_* bitmask (see <wiiuse/wpad.h>).
 * "Place" has two independently-bindable slots because the default scheme
 * fires the same action from either A or B; the same is true of the menu
 * toggle, which defaults to either - or +.
 */
typedef struct {
    u8  volume;              /* 0-100 master volume; wired up once audio is added */
    u32 keyPlacePrimary;     /* default: A     */
    u32 keyPlaceSecondary;   /* default: B     */
    u32 keyMoveRight;        /* default: 1     */
    u32 keyMoveLeft;         /* default: 2     */
    u32 keyMenuPrimary;      /* default: PLUS  */
    u32 keyMenuSecondary;    /* default: MINUS */
} Settings;

/** Reset to the documented default control scheme and 80% volume. */
void Settings_SetDefaults(Settings* settings);

/** Load from SETTINGS_PATH. Falls back to (and re-fills) defaults for any
 *  field that is missing, unparseable, or if the file does not exist yet. */
void Settings_Load(Settings* settings);

/** Best-effort save to SETTINGS_PATH. Creates SETTINGS_DIR if needed.
 *  Returns false if there is no writable SD card -- callers should treat
 *  that as non-fatal (settings simply stay in memory for this session). */
bool Settings_Save(const Settings* settings);

/** Human-readable name for a WPAD_BUTTON_* mask ("A", "PLUS", ...).
 *  Returns "?" for a mask that isn't in the assignable table. */
const char* Settings_ButtonName(u32 mask);

/** Reverse lookup used while parsing the settings file. Returns 0 (no match)
 *  if the name isn't recognized -- callers should keep the previous/default
 *  value in that case. */
u32 Settings_ButtonFromName(const char* name);

/** Number of entries in, and accessor for, the physical buttons offered by
 *  the Controls remap menu (HOME is intentionally excluded -- it always
 *  exits the game and is never reassignable). */
int Settings_RemappableButtonCount(void);
u32 Settings_RemappableButtonAt(int index);

#ifdef __cplusplus
}
#endif

#endif /* PVZ_SETTINGS_H */
