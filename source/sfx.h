/**
 * sfx.h - Sound effects (and short music stings) for Plants vs. Zombies Wii.
 *
 * Every sound lives in assets/sfx/<name>.wav as 16-bit PCM (mono, 22.05 kHz
 * -- converted from the original .ogg pack by scripts/convert_sfx.py, because
 * the Wii build has no Ogg decoder). Sounds are loaded on first use (a core
 * set is preloaded in Sfx_Init) and mixed on libogc ASND voices 1-15, so any
 * number can overlap with the MP3 background music (MP3Player).
 *
 * To add a sound: drop <name>.wav in assets/sfx/, add `X(<NAME>, <name>)` to
 * SFX_LIST below (the enum SFX_<NAME> is generated from it), then call
 * Sfx_Play(SFX_<NAME>) wherever it should be heard.
 */
#ifndef PVZ_SFX_H
#define PVZ_SFX_H

#include <gctypes.h>

#define SFX_LIST(X) \
    X(RVTHROW, RVthrow) \
    X(ACHIEVEMENT, achievement) \
    X(AWOOGA, awooga) \
    X(BALLOON_POP, balloon_pop) \
    X(BALLOONINFLATE, ballooninflate) \
    X(BASKETBALL, basketball) \
    X(BIGCHOMP, bigchomp) \
    X(BLEEP, bleep) \
    X(BLOVER, blover) \
    X(BOING, boing) \
    X(BONK, bonk) \
    X(BOSSBOULDERATTACK, bossboulderattack) \
    X(BOSSEXPLOSION, bossexplosion) \
    X(BOWLING, bowling) \
    X(BOWLINGIMPACT, bowlingimpact) \
    X(BOWLINGIMPACT2, bowlingimpact2) \
    X(BUGSPRAY, bugspray) \
    X(BUNGEE_SCREAM, bungee_scream) \
    X(BUNGEE_SCREAM2, bungee_scream2) \
    X(BUNGEE_SCREAM3, bungee_scream3) \
    X(BUTTER, butter) \
    X(BUTTONCLICK, buttonclick) \
    X(BUZZER, buzzer) \
    X(CERAMIC, ceramic) \
    X(CHERRYBOMB, cherrybomb) \
    X(CHIME, chime) \
    X(CHOMP, chomp) \
    X(CHOMP2, chomp2) \
    X(CHOMPSOFT, chompsoft) \
    X(COBLAUNCH, coblaunch) \
    X(COFFEE, coffee) \
    X(COIN, coin) \
    X(CRAZYDAVECRAZY, crazydavecrazy) \
    X(CRAZYDAVEEXTRALONG1, crazydaveextralong1) \
    X(CRAZYDAVEEXTRALONG2, crazydaveextralong2) \
    X(CRAZYDAVEEXTRALONG3, crazydaveextralong3) \
    X(CRAZYDAVELONG1, crazydavelong1) \
    X(CRAZYDAVELONG2, crazydavelong2) \
    X(CRAZYDAVELONG3, crazydavelong3) \
    X(CRAZYDAVESCREAM, crazydavescream) \
    X(CRAZYDAVESCREAM2, crazydavescream2) \
    X(CRAZYDAVESHORT1, crazydaveshort1) \
    X(CRAZYDAVESHORT2, crazydaveshort2) \
    X(CRAZYDAVESHORT3, crazydaveshort3) \
    X(DANCER, dancer) \
    X(DIAMOND, diamond) \
    X(DIGGER_ZOMBIE, digger_zombie) \
    X(DIRT_RISE, dirt_rise) \
    X(DOLPHIN_APPEARS, dolphin_appears) \
    X(DOLPHIN_BEFORE_JUMPING, dolphin_before_jumping) \
    X(DOOMSHROOM, doomshroom) \
    X(EVILLAUGH, evillaugh) \
    X(EXPLOSION, explosion) \
    X(FERTILIZER, fertilizer) \
    X(FINALFANFARE, finalfanfare) \
    X(FINALWAVE, finalwave) \
    X(FIREPEA, firepea) \
    X(FLOOP, floop) \
    X(FROZEN, frozen) \
    X(FUME, fume) \
    X(GARGANTUAR_THUMP, gargantuar_thump) \
    X(GARGANTUDEATH, gargantudeath) \
    X(GRASSSTEP, grassstep) \
    X(GRAVEBUSTERCHOMP, gravebusterchomp) \
    X(GRAVEBUTTON, gravebutton) \
    X(GRAVESTONE_RUMBLE, gravestone_rumble) \
    X(GROAN, groan) \
    X(GROAN2, groan2) \
    X(GROAN3, groan3) \
    X(GROAN4, groan4) \
    X(GROAN5, groan5) \
    X(GROAN6, groan6) \
    X(GULP, gulp) \
    X(HATCHBACK_CLOSE, hatchback_close) \
    X(HATCHBACK_OPEN, hatchback_open) \
    X(HUGEWAVE, hugewave) \
    X(HYDRAULIC, hydraulic) \
    X(HYDRAULIC_SHORT, hydraulic_short) \
    X(IGNITE, ignite) \
    X(IGNITE2, ignite2) \
    X(IMP, imp) \
    X(IMP2, imp2) \
    X(JACK_SURPRISE, jack_surprise) \
    X(JACK_SURPRISE2, jack_surprise2) \
    X(JACKINTHEBOX, jackinthebox) \
    X(JALAPENO, jalapeno) \
    X(JUICY, juicy) \
    X(KERNELPULT, kernelpult) \
    X(KERNELPULT2, kernelpult2) \
    X(LADDER_ZOMBIE, ladder_zombie) \
    X(LAWNMOWER, lawnmower) \
    X(LIGHTFILL, lightfill) \
    X(LIMBS_POP, limbs_pop) \
    X(LOADINGBAR_FLOWER, loadingbar_flower) \
    X(LOADINGBAR_ZOMBIE, loadingbar_zombie) \
    X(LOSEMUSIC, losemusic) \
    X(LOWGROAN, lowgroan) \
    X(LOWGROAN2, lowgroan2) \
    X(MAGNETSHROOM, magnetshroom) \
    X(MELONIMPACT, melonimpact) \
    X(MELONIMPACT2, melonimpact2) \
    X(MINDCONTROLLED, mindcontrolled) \
    X(MONEYFALLS, moneyfalls) \
    X(NEWSPAPER_RARRGH, newspaper_rarrgh) \
    X(NEWSPAPER_RARRGH2, newspaper_rarrgh2) \
    X(NEWSPAPER_RIP, newspaper_rip) \
    X(PAPER, paper) \
    X(PAUSE, pause) \
    X(PHONOGRAPH, phonograph) \
    X(PLANT, plant) \
    X(PLANT2, plant2) \
    X(PLANT_WATER, plant_water) \
    X(PLANTERN, plantern) \
    X(PLANTGROW, plantgrow) \
    X(PLASTICHIT, plastichit) \
    X(PLASTICHIT2, plastichit2) \
    X(POGO_ZOMBIE, pogo_zombie) \
    X(POINTS, points) \
    X(POLEVAULT, polevault) \
    X(POOL_CLEANER, pool_cleaner) \
    X(PORTAL, portal) \
    X(POTATO_MINE, potato_mine) \
    X(PRIZE, prize) \
    X(PUFF, puff) \
    X(RAIN, rain) \
    X(READYSETPLANT, readysetplant) \
    X(REVERSE_EXPLOSION, reverse_explosion) \
    X(ROLL_IN, roll_in) \
    X(SCREAM, scream) \
    X(SEEDLIFT, seedlift) \
    X(SHIELDHIT, shieldhit) \
    X(SHIELDHIT2, shieldhit2) \
    X(SHOOP, shoop) \
    X(SHOVEL, shovel) \
    X(SIREN, siren) \
    X(SLOTMACHINE, slotmachine) \
    X(SLURP, slurp) \
    X(SNOW_PEA_SPARKLES, snow_pea_sparkles) \
    X(SPLAT, splat) \
    X(SPLAT2, splat2) \
    X(SPLAT3, splat3) \
    X(SQUASH_HMM, squash_hmm) \
    X(SQUASH_HMM2, squash_hmm2) \
    X(SUKHBIR, sukhbir) \
    X(SUKHBIR2, sukhbir2) \
    X(SUKHBIR3, sukhbir3) \
    X(SUKHBIR4, sukhbir4) \
    X(SUKHBIR5, sukhbir5) \
    X(SUKHBIR6, sukhbir6) \
    X(SWING, swing) \
    X(TAP, tap) \
    X(TAP2, tap2) \
    X(TAPGLASS, tapglass) \
    X(THROW, throw) \
    X(THROW2, throw2) \
    X(THUNDER, thunder) \
    X(VASE_BREAKING, vase_breaking) \
    X(WAKEUP, wakeup) \
    X(WATERING, watering) \
    X(WINMUSIC, winmusic) \
    X(YUCK, yuck) \
    X(YUCK2, yuck2) \
    X(ZAMBONI, zamboni) \
    X(ZOMBAQUARIUM_DIE, zombaquarium_die) \
    X(ZOMBIE_ENTERING_WATER, zombie_entering_water) \
    X(ZOMBIE_FALLING_1, zombie_falling_1) \
    X(ZOMBIE_FALLING_2, zombie_falling_2) \
    X(ZOMBIESPLASH, zombiesplash)

typedef enum {
#define SFX_ENUM(ID, file) SFX_##ID,
    SFX_LIST(SFX_ENUM)
#undef SFX_ENUM
    SFX_COUNT
} SfxId;

#ifdef __cplusplus
extern "C" {
#endif

/** Call once after ASND_Init() and after the working directory is set. */
void Sfx_Init(void);
void Sfx_Shutdown(void);
/** Call once per frame (Audio_Update does). */
void Sfx_Update(void);

/** Play at full volume / at `vol` (0-255), both scaled by the master volume. */
void Sfx_Play(SfxId id);
void Sfx_PlayVol(SfxId id, u8 vol);
/** Play `id` after `frames` video frames (60 = one second). */
void Sfx_PlayDelayed(SfxId id, u32 frames, u8 vol);
/** Play one of ids[0..n-1], picked at random. */
void Sfx_PlayRandom(const SfxId* ids, int n, u8 vol);
/** Stop every sound effect that is currently playing. */
void Sfx_StopAll(void);
/** 0-255, follows the options-menu volume (see Audio_SetMasterVolume). */
void Sfx_SetMasterVolume(u8 volume);
/** File name (no extension) of a sound, for logging. */
const char* Sfx_Name(SfxId id);

#ifdef __cplusplus
}
#endif

#endif /* PVZ_SFX_H */
