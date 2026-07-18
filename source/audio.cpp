#include "audio.h"
#include "survival.h"
#include "minigame.h"
#include <string.h>
#include <stdlib.h>
#include <stdio.h>
#include <asndlib.h>
#include <mp3player.h>
#include <fat.h>
#include <ogc/lwp_watchdog.h>
#include <unistd.h>

/* -------------------------------------------------------------------------- */
/* BGM state                                                                  */
/* -------------------------------------------------------------------------- */
/* The music assets under assets/music/*.wav are actually MP3 data (despite
 * the .wav extension) -- confirmed by their ID3/MPEG headers, not RIFF/WAVE.
 * Re-encoding them to raw PCM WAV would balloon ~22MB of compressed audio
 * into 160MB+, far past what fits in a Wii's memory, so instead of decoding
 * WAV ourselves we hand the file's raw bytes to libogc's own MP3Player
 * (libmad-based decoder, see mp3player.h / -lmad in the Makefile).
 *
 * MP3Player_PlayBuffer() decodes from the buffer incrementally as it plays,
 * so the buffer must stay alive for as long as playback continues -- it's
 * kept in s_bgmData/s_bgmDataSize until Audio_StopBGM() or the next
 * Audio_PlayBGM() call frees it. */
static u8* s_bgmData = NULL;
static u32 s_bgmDataSize = 0;
static bool s_bgmLooping = false;
static u8   s_masterVolume = 200; /* 0-255, default 200 ~= 78% */

/* -------------------------------------------------------------------------- */
/* Public API                                                                 */
/* -------------------------------------------------------------------------- */

void Audio_Init(void)
{
    ASND_Init();
    ASND_Pause(0);
    MP3Player_Init();
    s_masterVolume = 200;
    MP3Player_Volume(s_masterVolume);
    s_bgmData = NULL;
    s_bgmDataSize = 0;
    s_bgmLooping = false;
}

void Audio_Shutdown(void)
{
    Audio_StopBGM();
    ASND_End();
}

void Audio_Update(void)
{
    /* MP3Player has no built-in loop option -- restart playback each time
     * the current track finishes so BGM loops for as long as the level is
     * active. */
    if (s_bgmLooping && s_bgmData && !MP3Player_IsPlaying())
    {
        MP3Player_PlayBuffer(s_bgmData, (s32)s_bgmDataSize, NULL);
    }
}

void Audio_PlayBGM(const char* wavPath)
{
    if (!wavPath) return;

    Audio_StopBGM();

    FILE* f = fopen(wavPath, "rb");
    if (!f) return;

    if (fseek(f, 0, SEEK_END) != 0)
    {
        fclose(f);
        return;
    }
    long fileSize = ftell(f);
    if (fileSize <= 0)
    {
        fclose(f);
        return;
    }
    rewind(f);

    u8* buffer = (u8*)malloc((size_t)fileSize);
    if (!buffer)
    {
        fclose(f);
        return;
    }

    size_t readBytes = fread(buffer, 1, (size_t)fileSize, f);
    fclose(f);

    if (readBytes != (size_t)fileSize)
    {
        free(buffer);
        return;
    }

    s_bgmData = buffer;
    s_bgmDataSize = (u32)fileSize;
    s_bgmLooping = true;

    MP3Player_Volume(s_masterVolume);
    MP3Player_PlayBuffer(s_bgmData, (s32)s_bgmDataSize, NULL);
}

void Audio_StopBGM(void)
{
    MP3Player_Stop();
    /* Wait for the MP3 decoder to fully finish before freeing the buffer,
     * otherwise the interrupt-driven ASND/AI thread may still be reading
     * from s_bgmData after we free it (use-after-free crash). */
    int wait = 0;
    while (MP3Player_IsPlaying() && wait < 100)
    {
        usleep(1000);
        wait++;
    }
    s_bgmLooping = false;
    if (s_bgmData)
    {
        free(s_bgmData);
        s_bgmData = NULL;
    }
    s_bgmDataSize = 0;
}

bool Audio_IsBGMPlaying(void)
{
    return MP3Player_IsPlaying();
}

void Audio_PlayLevelBGM(u8 levelIndex)
{
    if (Survival_IsSurvival(levelIndex))
    {
        SurvivalType st = Survival_GetType(levelIndex);
        const char* path = "assets/music/world 1.wav";
        if (st >= SURVIVAL_NIGHT_EASY && st <= SURVIVAL_NIGHT_HARD)
            path = "assets/music/world 2.wav";
        else if (st >= SURVIVAL_POOL_EASY && st <= SURVIVAL_POOL_HARD)
            path = "assets/music/world 3.wav";
        else if (st >= SURVIVAL_FOG_EASY && st <= SURVIVAL_FOG_HARD)
            path = "assets/music/world 4.wav";
        else if (st >= SURVIVAL_ROOF_EASY && st <= SURVIVAL_ENDLESS)
            path = "assets/music/world 5.wav";
        Audio_PlayBGM(path);
        return;
    }

    if (Minigame_IsMinigame(levelIndex))
    {
        Audio_PlayBGM("assets/music/world 1.wav");
        return;
    }

    u8 world = levelIndex / 10;
    if (world > 4) world = 4;

    if (levelIndex == 49)
    {
        Audio_PlayBGM("assets/music/last_battle.wav");
        return;
    }

    const char* worldFiles[5] = {
        "assets/music/world 1.wav",
        "assets/music/world 2.wav",
        "assets/music/world 3.wav",
        "assets/music/world 4.wav",
        "assets/music/world 5.wav"
    };
    Audio_PlayBGM(worldFiles[world]);
}

void Audio_SetMasterVolume(u8 volume)
{
    s_masterVolume = volume;
    MP3Player_Volume(s_masterVolume);
}

u8 Audio_GetMasterVolume(void)
{
    return s_masterVolume;
}
