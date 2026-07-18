/**
 * combat.h - Shooter plants (Peashooter/Repeater/Snow Pea) firing projectiles
 * at zombies, and Cherry Bomb's instant blast. Purely additive to game.cpp's
 * existing grid/planting logic.
 */
#ifndef PVZ_COMBAT_H
#define PVZ_COMBAT_H

#ifdef __cplusplus
extern "C" {
#endif

#include "constants.h"
#include "types.h"

struct GameContext; /* defined in game.h */

/** Reset the projectile pool (call once, from Game_Reset). */
void Combat_ResetAll(struct GameContext* game);

/**
 * Advance one frame: shooter plants tick down and fire when a zombie is
 * anywhere ahead in their row, and active projectiles move right and apply
 * damage on the first zombie they reach.
 */
void Combat_Update(struct GameContext* game);

/** Cherry Bomb's instant 3x3 blast, centered on (col,row). Call once, right
 *  when the Cherry Bomb is placed (it has no ongoing behavior afterwards). */
void Combat_TriggerCherryBomb(struct GameContext* game, s8 col, s8 row);

#ifdef __cplusplus
}
#endif

#endif /* PVZ_COMBAT_H */
