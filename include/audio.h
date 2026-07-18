#ifndef PVZ_AUDIO_H
#define PVZ_AUDIO_H

#ifdef __cplusplus
extern "C" {
#endif

#include <gctypes.h>

#define AUDIO_BGM_VOICE 0
#define AUDIO_SFX_VOICE_1 1
#define AUDIO_SFX_VOICE_2 2
#define AUDIO_SFX_VOICE_3 3

void Audio_Init(void);
void Audio_Shutdown(void);
void Audio_Update(void);

void Audio_PlayBGM(const char* wavPath);
void Audio_StopBGM(void);
bool Audio_IsBGMPlaying(void);

void Audio_PlayLevelBGM(u8 levelIndex);
void Audio_SetMasterVolume(u8 volume);
u8   Audio_GetMasterVolume(void);

#ifdef __cplusplus
}
#endif

#endif
