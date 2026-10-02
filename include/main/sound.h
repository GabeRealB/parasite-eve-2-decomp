#ifndef MAIN_SOUND_H
#define MAIN_SOUND_H

#include "types.h"

#include "main/sound_ids.h"

#include "main/sound_types.h"

extern s32 D_800820E0;

extern s16 D_800820E4;

extern s16 D_800820E6;

void Snd_ApplyVolumeTable(s32 arg0);

s32 LinInterp_Apply(LinInterp* arg0, s32 arg1);

s32 SndEvt_EnqueueType1(s32 arg0, s32 arg1);

s32 SndEvt_EnqueueType2(s32 arg0, s32 arg1);

s32 Midi_IsBusy(s32 arg0);

s32 Midi_GetMasterVolume(void);

s32 SndEvt_EnqueueType5(s32 arg0, s32 arg1);

void Snd_InitFromStage(s32 arg0, s32 arg1);

s32 SndEvt_EnqueueType6(s32 arg0, s32 arg1, s32 arg2);

void SndEvt_EnqueueTypeA(s32 arg0, s32 arg1, s32 arg2);

void SndEvt_EnqueueTypeB(s32 arg0, s32 arg1);

void SndBank_SetEnableFlags(s32 arg0, s32 arg1);

s32 SndVoice_HasActiveId(s32 arg0);

void SndEvt_EnqueueTypeD(void);

void SndEvt_EnqueueTypeE(void);

void SndEvt_EnqueueType7(s32 arg0, s32 arg1);

void SndEvt_EnqueueType8(s32 arg0);

void SndEvt_EnqueueType9(s32 arg0);

void Snd_SetModeFlag(s32 arg0);

#endif // MAIN_SOUND_H
