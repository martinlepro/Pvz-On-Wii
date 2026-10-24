/**
 * sfx.cpp - Sound-effect playback (see sfx.h).
 *
 * Samples are decoded once into 32-byte-aligned big-endian PCM buffers (what
 * ASND wants) and kept for the lifetime of the program -- the whole pack is
 * ~12 MB, the Wii has 88 MB. ASND voice 0 is left alone; effects round-robin
 * over voices 1..15, so they can overlap with each other and with the MP3
 * background music.
 */
#include "sfx.h"
#include <asndlib.h>
#include <ogc/cache.h>
#include <malloc.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

namespace {

#define SFX_FILE(ID, file) #file,
const char* const kNames[SFX_COUNT] = { SFX_LIST(SFX_FILE) };
#undef SFX_FILE

struct Sample {
    s16* data;        /* big-endian PCM, 32-byte aligned, zero padded            */
    u32  bytes;       /* padded to a multiple of 32                                */
    u32  rate;
    s32  format;      /* VOICE_MONO_16BIT / VOICE_STEREO_16BIT                     */
    bool tried;       /* load attempted (so a missing file is only looked up once) */
};

Sample s_samples[SFX_COUNT];
u32    s_lastFrame[SFX_COUNT];   /* frame+1 of the last play, 0 = never            */
u32    s_frame      = 0;
u8     s_master     = 200;
int    s_nextVoice  = 1;
bool   s_ready      = false;

const int kFirstVoice = 1, kLastVoice = 15;

struct Pending { SfxId id; u32 left; u8 vol; bool used; };
Pending s_pending[16];

inline u32 Rd32(const u8* p) { return (u32)p[0] | ((u32)p[1] << 8) | ((u32)p[2] << 16) | ((u32)p[3] << 24); }
inline u16 Rd16(const u8* p) { return (u16)(p[0] | (p[1] << 8)); }

/* Reads a whole file; tries the cwd-relative path first, then the same
 * install locations the texture loader knows about. */
u8* ReadFile(const char* rel, u32* outSize)
{
    static const char* kRoots[] = { "", "sd:/apps/pvz_wii/", "usb:/apps/pvz_wii/", "dvd:/", "wiidisc:/" };
    char path[256];
    for (unsigned i = 0; i < sizeof(kRoots) / sizeof(kRoots[0]); ++i)
    {
        snprintf(path, sizeof(path), "%s%s", kRoots[i], rel);
        FILE* f = fopen(path, "rb");
        if (!f) continue;
        fseek(f, 0, SEEK_END);
        long sz = ftell(f);
        rewind(f);
        if (sz <= 44 || sz > 8 * 1024 * 1024) { fclose(f); continue; }
        u8* buf = (u8*)malloc((size_t)sz);
        if (buf && fread(buf, 1, (size_t)sz, f) == (size_t)sz)
        {
            fclose(f);
            *outSize = (u32)sz;
            return buf;
        }
        free(buf);
        fclose(f);
    }
    return NULL;
}

bool Load(SfxId id)
{
    Sample& s = s_samples[id];
    if (s.data) return true;
    if (s.tried) return false;
    s.tried = true;

    char rel[96];
    snprintf(rel, sizeof(rel), "assets/sfx/%s.wav", kNames[id]);
    u32 size = 0;
    u8* file = ReadFile(rel, &size);
    if (!file) return false;

    /* RIFF/WAVE: find "fmt " and "data" */
    bool ok = size > 12 && memcmp(file, "RIFF", 4) == 0 && memcmp(file + 8, "WAVE", 4) == 0;
    u32 rate = 0; u16 channels = 0, bits = 0, tag = 0;
    const u8* pcm = NULL; u32 pcmBytes = 0;
    for (u32 pos = 12; ok && pos + 8 <= size; )
    {
        u32 len = Rd32(file + pos + 4);
        const u8* body = file + pos + 8;
        if (memcmp(file + pos, "fmt ", 4) == 0 && len >= 16 && pos + 8 + len <= size)
        {
            tag = Rd16(body); channels = Rd16(body + 2); rate = Rd32(body + 4); bits = Rd16(body + 14);
        }
        else if (memcmp(file + pos, "data", 4) == 0)
        {
            pcm = body;
            pcmBytes = (pos + 8 + len > size) ? size - (pos + 8) : len;
            break;
        }
        pos += 8 + len + (len & 1);
    }
    if (!ok || !pcm || tag != 1 || bits != 16 || (channels != 1 && channels != 2) || rate == 0)
    {
        free(file);
        return false;
    }

    const u32 frameBytes = 2u * channels;
    pcmBytes -= pcmBytes % frameBytes;
    const u32 padded = (pcmBytes + 31u) & ~31u;
    s16* buf = (s16*)memalign(32, padded ? padded : 32);
    if (!buf) { free(file); return false; }
    memset(buf, 0, padded);
    /* little-endian WAV -> big-endian PowerPC */
    for (u32 i = 0; i < pcmBytes / 2; ++i)
    {
        u16 v = Rd16(pcm + i * 2);
        buf[i] = (s16)(u16)((v >> 8) | (v << 8));
    }
    free(file);
    DCFlushRange(buf, padded);

    s.data   = buf;
    s.bytes  = padded;
    s.rate   = rate;
    s.format = (channels == 2) ? VOICE_STEREO_16BIT : VOICE_MONO_16BIT;
    return true;
}

int PickVoice()
{
    for (int n = 0; n < kLastVoice - kFirstVoice + 1; ++n)
    {
        int v = s_nextVoice;
        if (++s_nextVoice > kLastVoice) s_nextVoice = kFirstVoice;
        if (ASND_StatusVoice(v) == SND_UNUSED)
            return v;
    }
    /* every voice busy: steal the next one in the rotation */
    int v = s_nextVoice;
    if (++s_nextVoice > kLastVoice) s_nextVoice = kFirstVoice;
    ASND_StopVoice(v);
    return v;
}

/* Sounds worth having in memory before the first frame (no disc/SD hitch). */
const SfxId kCore[] = {
    SFX_BLEEP, SFX_BUTTONCLICK, SFX_TAP, SFX_TAP2, SFX_PAUSE, SFX_SEEDLIFT, SFX_PLANT, SFX_PLANT2,
    SFX_BUZZER, SFX_POINTS, SFX_SHOVEL, SFX_COIN, SFX_SPLAT, SFX_SPLAT2, SFX_SPLAT3,
    SFX_SNOW_PEA_SPARKLES, SFX_IGNITE, SFX_IGNITE2, SFX_PLASTICHIT, SFX_PLASTICHIT2, SFX_SHIELDHIT,
    SFX_SHIELDHIT2, SFX_THROW, SFX_THROW2, SFX_KERNELPULT, SFX_KERNELPULT2, SFX_PUFF, SFX_FUME,
    SFX_CHOMP, SFX_CHOMP2, SFX_CHOMPSOFT, SFX_GULP, SFX_GROAN, SFX_GROAN2, SFX_GROAN3, SFX_GROAN4,
    SFX_GROAN5, SFX_GROAN6, SFX_LOWGROAN, SFX_LOWGROAN2, SFX_LIMBS_POP, SFX_ZOMBIE_FALLING_1,
    SFX_ZOMBIE_FALLING_2, SFX_CHERRYBOMB, SFX_LAWNMOWER, SFX_READYSETPLANT, SFX_HUGEWAVE,
    SFX_FINALWAVE, SFX_WINMUSIC, SFX_LOSEMUSIC, SFX_SCREAM
};

} // namespace

extern "C" void Sfx_Init(void)
{
    memset(s_lastFrame, 0, sizeof(s_lastFrame));
    memset(s_pending, 0, sizeof(s_pending));
    s_frame = 0;
    s_nextVoice = kFirstVoice;
    s_ready = true;
    for (unsigned i = 0; i < sizeof(kCore) / sizeof(kCore[0]); ++i)
        Load(kCore[i]);
}

extern "C" void Sfx_Shutdown(void)
{
    Sfx_StopAll();
    for (int i = 0; i < SFX_COUNT; ++i)
    {
        if (s_samples[i].data) free(s_samples[i].data);
        memset(&s_samples[i], 0, sizeof(Sample));
    }
    s_ready = false;
}

extern "C" void Sfx_Update(void)
{
    ++s_frame;
    for (int i = 0; i < 16; ++i)
    {
        Pending& p = s_pending[i];
        if (!p.used) continue;
        if (p.left > 0) { --p.left; continue; }
        p.used = false;
        Sfx_PlayVol(p.id, p.vol);
    }
}

extern "C" void Sfx_PlayDelayed(SfxId id, u32 frames, u8 vol)
{
    if (!s_ready) return;
    if (frames == 0) { Sfx_PlayVol(id, vol); return; }
    for (int i = 0; i < 16; ++i)
        if (!s_pending[i].used)
        {
            s_pending[i].used = true; s_pending[i].id = id;
            s_pending[i].left = frames; s_pending[i].vol = vol;
            return;
        }
}

extern "C" void Sfx_PlayVol(SfxId id, u8 vol)
{
    if (!s_ready || (int)id < 0 || (int)id >= SFX_COUNT) return;
    /* the same sound at most once per frame (e.g. 4 gatling peas hitting at once) */
    if (s_lastFrame[id] == s_frame + 1) return;
    if (!Load(id)) return;
    s_lastFrame[id] = s_frame + 1;

    const Sample& s = s_samples[id];
    u32 v = ((u32)vol * (u32)s_master) / 255u;
    if (v > 255) v = 255;
    int voice = PickVoice();
    ASND_SetVoice(voice, s.format, (s32)s.rate, 0, (void*)s.data, (s32)s.bytes, (s32)v, (s32)v, NULL);
}

extern "C" void Sfx_Play(SfxId id) { Sfx_PlayVol(id, 255); }

extern "C" void Sfx_PlayRandom(const SfxId* ids, int n, u8 vol)
{
    if (!ids || n <= 0) return;
    Sfx_PlayVol(ids[rand() % n], vol);
}

extern "C" void Sfx_StopAll(void)
{
    memset(s_pending, 0, sizeof(s_pending));
    for (int v = kFirstVoice; v <= kLastVoice; ++v)
        ASND_StopVoice(v);
}

extern "C" void Sfx_SetMasterVolume(u8 volume) { s_master = volume; }

extern "C" const char* Sfx_Name(SfxId id)
{
    return ((int)id >= 0 && (int)id < SFX_COUNT) ? kNames[id] : "";
}
