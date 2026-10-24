/**
 * save.h - Progress save system.
 *
 * What is saved (sd:/apps/pvz_wii/save.cfg, plain text):
 *   ADV_LEVEL  the Adventure level the title screen's Adventure button starts
 *              (0-based, 0..TOTAL_LEVELS-1; shown there as "Level <world>-<n>")
 *   ADV_DONE   1 once the final level (5-10) has been cleared
 * and, at the same moments, the coin total picked up in levels (coins.cfg).
 * Inventory items (items.cfg), the Zen Garden (zen.cfg) and the options
 * (settings.cfg) already persist themselves.
 *
 * WHEN it saves -- the three moments the player cares about:
 *   * a level is COMPLETED  -> Save_OnLevelWon()   (progress moves to the next level)
 *   * a level is LOST       -> Save_OnLevelLost()  (progress stays on that level)
 *   * a level is LEFT / the game is quit -> Save_OnExit()
 * Nothing is written during play, so there are no SD-card hitches in a level.
 *
 * Without an SD card every call is a harmless no-op that keeps the progress in
 * memory for the current session.
 */
#ifndef PVZ_SAVE_H
#define PVZ_SAVE_H

#ifdef __cplusplus
extern "C" {
#endif

/** Level (0-based) the Adventure button starts. Loads the save on first use. */
int  Save_GetAdventureLevel(void);

/** True once the whole Adventure (level 5-10) has been completed. */
int  Save_AdventureCompleted(void);

/** Called whenever a level starts (Level_Start). */
void Save_NoteLevelStarted(unsigned char levelIndex, int isAdventure);

/** A level was completed: advances Adventure progress, then saves. */
void Save_OnLevelWon(unsigned char levelIndex, int isAdventure);

/** A level was lost: keeps Adventure progress on that level, then saves. */
void Save_OnLevelLost(unsigned char levelIndex, int isAdventure);

/** The player left the level for the title screen, or the game is quitting. */
void Save_OnExit(void);

#ifdef __cplusplus
}
#endif

#endif /* PVZ_SAVE_H */
