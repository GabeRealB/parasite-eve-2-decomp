#ifndef MAIN_PRIVATE_CDAUDIO_H
#define MAIN_PRIVATE_CDAUDIO_H

#include <psyq/sys/types.h>
#include <psyq/libspu.h>

#include "types.h"

extern u8 D_80068AF0[];

extern u16 Spu_SemitonePitchTable[];

extern u16 Spu_FinePitchTable[];

extern u16 Snd_PanGainTable[];

extern u16 Snd_VelocityGainTable[];

s32 CdAudio_Begin(void);

void CdAudio_Init(void);

void CdAudio_Tick(void);

/// Copies one complete voice attribute record into the pending SPU update.
void CdAudio_CopyVoiceData(s8 voice, const SpuVoiceAttr* attr);

void CdAudio_AllocVoices(s8* arg0, s8* arg1);

#endif // MAIN_PRIVATE_CDAUDIO_H
