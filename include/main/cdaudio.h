#ifndef MAIN_CDAUDIO_H
#define MAIN_CDAUDIO_H

#include "types.h"

#include "main/cdaudio_types.h"

extern volatile CdAudioProgress CdAudio_Phase;

s32 CdAudio_StartTrack(s32 sector, s32 volumeIndex);

s32 CdAudio_JumpToSector(s32 arg0);

s32 CdAudio_RequestStopB(void);

#endif // MAIN_CDAUDIO_H
