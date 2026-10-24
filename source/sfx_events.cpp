/**
 * sfx_events.cpp - Turns what happens on the lawn into sound effects.
 *
 * Instead of sprinkling sound calls through every zombie / projectile code
 * path, Sfx_PollGame() is called once per simulated frame (end of
 * Game_Update) and compares the game state with a snapshot of the previous
 * frame: a zombie that lost health was hit, one that just became DYING fell
 * over, a projectile that appeared was fired, and so on. Only things that
 * leave no trace in the state (planting an instant plant, picking up a sun,
 * a lawn mower starting...) are triggered by explicit Sfx_Play() calls in the
 * respective files.
 */
#include "game.h"
#include "sfx.h"
#include "audio.h"
#include "survival.h"
#include "minigame.h"
#include <stdlib.h>
#include <string.h>

namespace {

struct ZSnap {
    bool active;
    ZombieType type;
    ZombieRuntimeState state;
    s16  health;
    s16  shield;
    u32  slow;
    bool movingRight;
    bool special;
    f32  speedMul;
};

struct PSnap {
    bool active;
    ProjectileKind kind;
    f32  x;
    u8   life;
};

ZSnap s_z[MAX_ZOMBIES];
PSnap s_p[MAX_PROJECTILES];
PlantType s_plant[GRID_ROWS][GRID_COLS];
bool s_ladder[GRID_ROWS][GRID_COLS];
GameState s_state    = GAME_STATE_SELECTING;
bool  s_hugePending  = false;
bool  s_valid        = false;
int   s_groanTimer   = 0;

inline bool Dying(const ZSnap& z) { return z.state == ZSTATE_DYING; }

int RandRange(int lo, int hi) { return lo + (rand() % (hi - lo + 1)); }

void Snapshot(const GameContext* g)
{
    for (int i = 0; i < MAX_ZOMBIES; ++i)
    {
        const Zombie& z = g->zombies[i];
        s_z[i].active = z.active;
        s_z[i].type = z.type;
        s_z[i].state = z.state;
        s_z[i].health = z.health;
        s_z[i].shield = z.shieldHealth;
        s_z[i].slow = z.slowFramesLeft;
        s_z[i].movingRight = z.movingRight;
        s_z[i].special = z.specialFlag;
        s_z[i].speedMul = z.speedMul;
    }
    for (int i = 0; i < MAX_PROJECTILES; ++i)
    {
        const Projectile& p = g->projectiles[i];
        s_p[i].active = p.active;
        s_p[i].kind = p.kind;
        s_p[i].x = p.x;
        s_p[i].life = p.life;
    }
    for (int r = 0; r < GRID_ROWS; ++r)
        for (int c = 0; c < GRID_COLS; ++c)
        {
            s_plant[r][c]  = g->grid[r][c].plant;
            s_ladder[r][c] = g->grid[r][c].hasLadder;
        }
    s_state       = g->state;
    s_hugePending = g->hugeWavePending;
}

void ZombieSpawned(const Zombie& z)
{
    switch (z.type)
    {
        case ZOMBIE_MINER:          Sfx_Play(SFX_DIGGER_ZOMBIE); break;
        case ZOMBIE_POGO:           Sfx_Play(SFX_POGO_ZOMBIE); break;
        case ZOMBIE_ZOMBONI:        Sfx_Play(SFX_ZAMBONI); break;
        case ZOMBIE_BALLOON:        Sfx_Play(SFX_BALLOONINFLATE); break;
        case ZOMBIE_DOLPHIN_RIDER:  Sfx_Play(SFX_DOLPHIN_APPEARS); break;
        case ZOMBIE_JACK_IN_BOX:    Sfx_Play(SFX_JACKINTHEBOX); break;
        case ZOMBIE_DISCO:          Sfx_Play(SFX_DANCER); break;
        case ZOMBIE_YETI:           Sfx_Play(SFX_GROAN4); break;
        case ZOMBIE_IMP:
        {
            static const SfxId k[] = { SFX_IMP, SFX_IMP2 };
            Sfx_PlayRandom(k, 2, 255);
            break;
        }
        case ZOMBIE_GARGANTUAR:
        case ZOMBIE_RED_GARGANTUAR: Sfx_Play(SFX_GARGANTUAR_THUMP); break;
        default: break;
    }
}

void ZombieHit(const ZSnap& was, const Zombie& now)
{
    const int drop = (int)was.health - (int)now.health;
    const bool shieldHit = now.shieldHealth < was.shield;

    /* big one-shot damage (bombs, mowers, chomper swallow...) has its own sound */
    if (drop >= 300 && !shieldHit) return;

    static const SfxId kMetal[]   = { SFX_SHIELDHIT, SFX_SHIELDHIT2 };
    static const SfxId kPlastic[] = { SFX_PLASTICHIT, SFX_PLASTICHIT2 };
    static const SfxId kSplat[]   = { SFX_SPLAT, SFX_SPLAT2, SFX_SPLAT3 };

    if (shieldHit)                                  Sfx_PlayRandom(kMetal, 2, 220);
    else if (now.type == ZOMBIE_BUCKETHEAD)         Sfx_PlayRandom(kMetal, 2, 220);
    else if (now.type == ZOMBIE_CONEHEAD ||
             now.type == ZOMBIE_FOOTBALL ||
             now.type == ZOMBIE_GIGA_FOOTBALL)      Sfx_PlayRandom(kPlastic, 2, 220);
    else                                            Sfx_PlayRandom(kSplat, 3, 200);
}

void ZombieDied(const ZSnap& was, const Zombie& now)
{
    switch (now.type)
    {
        case ZOMBIE_GARGANTUAR:
        case ZOMBIE_RED_GARGANTUAR:
            Sfx_Play(SFX_GARGANTUDEATH);
            return;
        case ZOMBIE_BALLOON:
            if (was.state == ZSTATE_FLYING) { Sfx_Play(SFX_BALLOON_POP); return; }
            break;
        default: break;
    }
    static const SfxId k[] = { SFX_ZOMBIE_FALLING_1, SFX_ZOMBIE_FALLING_2 };
    Sfx_PlayRandom(k, 2, 230);
}

void PollZombies(const GameContext* g)
{
    bool anyAlive = false;

    for (int i = 0; i < MAX_ZOMBIES; ++i)
    {
        const Zombie& z = g->zombies[i];
        const ZSnap& was = s_z[i];

        if (z.active && z.state != ZSTATE_DYING) anyAlive = true;

        if (z.active && !was.active)
        {
            ZombieSpawned(z);
            continue;
        }
        if (!z.active || !was.active) continue;

        /* death */
        if (z.state == ZSTATE_DYING && !Dying(was))
        {
            ZombieDied(was, z);
            continue;
        }
        if (z.state == ZSTATE_DYING) continue;

        /* damage */
        if (z.health < was.health || z.shieldHealth < was.shield)
        {
            ZombieHit(was, z);

            /* an arm falls off at half health (rigged humanoids only) */
            const ZombieDef* d = Zombie_GetDef(z.type);
            if (d && (d->flags & ZFLAG_RIGGED_HUMANOID))
            {
                const s16 half = (s16)(d->health / 2);
                if (was.health > half && z.health <= half && z.health > 0)
                    Sfx_Play(SFX_LIMBS_POP);
            }
        }

        /* snow pea slow / butter stun */
        if (z.slowFramesLeft > was.slow + 1)
        {
            if (z.slowFramesLeft >= (u32)BUTTER_STUN_FRAMES - 2) Sfx_Play(SFX_BUTTER);
            else                                                  Sfx_Play(SFX_SNOW_PEA_SPARKLES);
        }

        /* hypnotised */
        if (z.movingRight && !was.movingRight)
            Sfx_Play(SFX_MINDCONTROLLED);

        /* state changes */
        if (z.state != was.state)
        {
            switch (z.state)
            {
                case ZSTATE_VAULTING:
                    Sfx_Play(z.type == ZOMBIE_DOLPHIN_RIDER ? SFX_DOLPHIN_BEFORE_JUMPING : SFX_POLEVAULT);
                    break;
                case ZSTATE_SURFACING:
                    Sfx_Play(SFX_DIRT_RISE);
                    break;
                case ZSTATE_SWIMMING:
                    Sfx_Play(SFX_ZOMBIE_ENTERING_WATER);
                    break;
                case ZSTATE_THROWING:
                    Sfx_Play(SFX_BASKETBALL);
                    break;
                default: break;
            }
        }

        /* one-shots flagged by the zombie code */
        if (z.type == ZOMBIE_GARGANTUAR || z.type == ZOMBIE_RED_GARGANTUAR)
        {
            if (z.specialFlag && !was.special) Sfx_Play(SFX_IMP2);   /* throws its imp */
        }
        if (z.type == ZOMBIE_NEWSPAPER && z.speedMul > was.speedMul + 0.01f)
        {
            static const SfxId k[] = { SFX_NEWSPAPER_RARRGH, SFX_NEWSPAPER_RARRGH2 };
            Sfx_PlayRandom(k, 2, 255);
        }

        /* chewing: one bite sound per second per group */
        if (z.state == ZSTATE_EATING && (z.animTimer % (ZOMBIE_EAT_INTERVAL_FRAMES * 2)) == 0)
        {
            static const SfxId k[] = { SFX_CHOMP, SFX_CHOMP2, SFX_CHOMPSOFT };
            Sfx_PlayRandom(k, 3, 150);
        }
    }

    /* idle groans while zombies are on the lawn */
    if (anyAlive)
    {
        if (--s_groanTimer <= 0)
        {
            static const SfxId k[] = { SFX_GROAN, SFX_GROAN2, SFX_GROAN3, SFX_GROAN4,
                                       SFX_GROAN5, SFX_GROAN6, SFX_LOWGROAN, SFX_LOWGROAN2 };
            Sfx_PlayRandom(k, 8, 110);
            s_groanTimer = RandRange(60 * 6, 60 * 14);
        }
    }
    else
        s_groanTimer = 60 * 4;
}

void PollProjectiles(const GameContext* g)
{
    for (int i = 0; i < MAX_PROJECTILES; ++i)
    {
        const Projectile& p = g->projectiles[i];
        const PSnap& was = s_p[i];

        if (p.active && !was.active)
        {
            switch (p.kind)
            {
                case PROJ_CABBAGE:
                case PROJ_MELON:
                case PROJ_WINTER:
                {
                    static const SfxId k[] = { SFX_THROW, SFX_THROW2 };
                    Sfx_PlayRandom(k, 2, 220);
                    break;
                }
                case PROJ_KERNEL:
                {
                    static const SfxId k[] = { SFX_KERNELPULT, SFX_KERNELPULT2 };
                    Sfx_PlayRandom(k, 2, 220);
                    break;
                }
                case PROJ_FUME:  Sfx_Play(SFX_FUME); break;
                case PROJ_SPORE: Sfx_PlayVol(SFX_PUFF, 200); break;
                default: break;
            }
        }
        else if (p.active && was.active)
        {
            /* a pea that passed a Torchwood */
            if (p.kind == PROJ_FIRE && was.kind != PROJ_FIRE)
                Sfx_PlayVol(SFX_FIREPEA, 230);
        }
        else if (!p.active && was.active)
        {
            /* consumed on a zombie (as opposed to flying off / running out of range) */
            const bool hit = was.life > 1 && was.x < (f32)GRID_COLS + 1.5f;
            if (hit && (p.kind == PROJ_MELON || p.kind == PROJ_WINTER))
            {
                static const SfxId k[] = { SFX_MELONIMPACT, SFX_MELONIMPACT2 };
                Sfx_PlayRandom(k, 2, 230);
            }
        }
    }
}

void PollGrid(const GameContext* g)
{
    for (int r = 0; r < (int)g->rowCount && r < GRID_ROWS; ++r)
        for (int c = 0; c < GRID_COLS; ++c)
        {
            const GridCell& cell = g->grid[r][c];

            /* a plant that vanished while a zombie was chewing in its cell */
            if (s_plant[r][c] != PLANT_NONE && cell.plant == PLANT_NONE)
            {
                if (s_plant[r][c] == PLANT_GARLIC)
                {
                    static const SfxId k[] = { SFX_YUCK, SFX_YUCK2 };   /* zombie bit a garlic */
                    Sfx_PlayRandom(k, 2, 255);
                    continue;
                }
                for (int i = 0; i < MAX_ZOMBIES; ++i)
                {
                    const Zombie& z = g->zombies[i];
                    if (z.active && z.state == ZSTATE_EATING && z.row == r && (int)z.x == c)
                    {
                        Sfx_PlayVol(SFX_GULP, 220);
                        break;
                    }
                }
            }
            if (cell.hasLadder && !s_ladder[r][c])
                Sfx_Play(SFX_LADDER_ZOMBIE);
        }
}

void PollWaves(const GameContext* g)
{
    if (g->hugeWavePending && !s_hugePending)
        Sfx_Play(SFX_AWOOGA);                       /* "A huge wave is approaching!" */
    else if (!g->hugeWavePending && s_hugePending)
        Sfx_Play(SFX_HUGEWAVE);                     /* ...and here they come        */
}

void PollState(const GameContext* g)
{
    if (g->state == s_state) return;

    if (g->state == GAME_STATE_LEVEL_WON)
    {
        Audio_StopBGM();
        Sfx_Play(g->currentLevelIndex == TOTAL_LEVELS - 1 && !Survival_IsSurvival(g->currentLevelIndex) &&
                 !Minigame_IsMinigame(g->currentLevelIndex) ? SFX_FINALFANFARE : SFX_WINMUSIC);
        Sfx_PlayDelayed(SFX_COIN, 90, 255);
        if (g->newRewardPlant != PLANT_NONE)
            Sfx_PlayDelayed(SFX_PRIZE, 150, 255);
    }
    else if (g->state == GAME_STATE_GAME_OVER)
    {
        Audio_StopBGM();
        Sfx_Play(SFX_SCREAM);
        Sfx_PlayDelayed(SFX_LOSEMUSIC, 100, 255);
    }
}

} // namespace

/* ------------------------------------------------------------------------ */

extern "C" void Sfx_ResetGame(const GameContext* g)
{
    s_valid = false;
    s_groanTimer = 60 * 6;
    if (g) { Snapshot(g); s_valid = true; }
}

extern "C" void Sfx_PollGame(const GameContext* g)
{
    if (!g) return;
    if (!s_valid)           /* first frame of a level: adopt the board silently */
    {
        Snapshot(g);
        s_valid = true;
        return;
    }
    if (g->state == GAME_STATE_PLAYING)
    {
        PollZombies(g);
        PollProjectiles(g);
        PollGrid(g);
        PollWaves(g);
    }
    PollState(g);
    Snapshot(g);
}

/** A seed was successfully planted (called from game.cpp). */
extern "C" void Sfx_PlantPlaced(PlantType type, bool onWater)
{
    if (type == PLANT_PLANTERN) { Sfx_Play(SFX_PLANTERN); return; }
    if (type == PLANT_LILY_PAD || type == PLANT_SEA_SHROOM || type == PLANT_TANGLE_KELP || onWater)
    {
        Sfx_Play(SFX_PLANT_WATER);
        return;
    }
    static const SfxId k[] = { SFX_PLANT, SFX_PLANT2 };
    Sfx_PlayRandom(k, 2, 255);
}

/** An instant plant just went off (called from combat.cpp). */
extern "C" void Sfx_PlantTriggered(PlantType type)
{
    switch (type)
    {
        case PLANT_CHERRY_BOMB: Sfx_Play(SFX_CHERRYBOMB); break;
        case PLANT_JALAPENO:    Sfx_Play(SFX_JALAPENO); break;
        case PLANT_DOOM_SHROOM: Sfx_Play(SFX_DOOMSHROOM); break;
        case PLANT_ICE_SHROOM:  Sfx_Play(SFX_FROZEN); break;
        case PLANT_POTATO_MINE: Sfx_Play(SFX_POTATO_MINE); break;
        case PLANT_SQUASH:
        {
            static const SfxId k[] = { SFX_SQUASH_HMM, SFX_SQUASH_HMM2 };
            Sfx_PlayRandom(k, 2, 255);
            break;
        }
        case PLANT_CHOMPER:     Sfx_Play(SFX_BIGCHOMP); break;
        case PLANT_MAGNET_SHROOM: Sfx_Play(SFX_MAGNETSHROOM); break;
        case PLANT_COFFEE_BEAN: Sfx_Play(SFX_COFFEE); break;
        case PLANT_BLOVER:      Sfx_Play(SFX_BLOVER); break;
        case PLANT_TANGLE_KELP: Sfx_Play(SFX_ZOMBIESPLASH); break;
        default: break;
    }
}
