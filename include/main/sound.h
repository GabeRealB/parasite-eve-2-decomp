#ifndef MAIN_SOUND_H
#define MAIN_SOUND_H

#include "types.h"

#include "main/sound_ids.h"

#include "main/sound_types.h"

extern s32 D_800820E0;

extern s16 D_800820E4;

extern s16 D_800820E6;

void Snd_ApplyVolumeTable(s32 arg0);

s32 LinInterp_Apply(LinInterp* ramp, s32 arg1);

s32 SndEvt_EnqueueType1(s32 arg0, s32 arg1);

s32 SndEvt_EnqueueType2(s32 arg0, s32 arg1);

s32 Midi_IsBusy(s32 arg0);

s32 Midi_GetMasterVolume(void);

s32 SndEvt_EnqueueType5(s32 arg0, s32 arg1);

void Snd_InitFromStage(s32 arg0, s32 arg1);

/// Script-start selectors that return unchanged without queuing an event.
enum {
    SOUND_SCRIPT_REQUEST_NO_OP   = 0,
    SOUND_SCRIPT_REQUEST_NO_OP_8 = 8
};

/// Admission failures returned by `sndEvtRequestScriptStart`.
enum {
    SOUND_SCRIPT_START_UNAVAILABLE    = -1, // Matching bank type is loading, or the event pool is full
    SOUND_SCRIPT_START_INVALID_ENTRY  = -2, // No matching bank descriptor, or entry index outside its image
    SOUND_SCRIPT_START_ENTRY_ABSENT   = -3, // Entry has no script in the bank image
    SOUND_SCRIPT_START_TYPE_DISABLED  = -4, // Bank type disabled and the entry does not bypass that gate
    SOUND_SCRIPT_START_REDUCED_VOLUME = -5  // Entry forbidden under the reduced-volume policy
};

/// Queues a deferred sound-script start and returns the original request id, or an admission failure.
///
/// `soundId` packs a 16-bit bank id above an 8-bit instance tag and 8-bit entry
/// index. Type-1 requests select the loaded type-1 bank while keeping both low
/// bytes. `SOUND_SCRIPT_REQUEST_NO_OP` and `SOUND_SCRIPT_REQUEST_NO_OP_8` return
/// unchanged without queuing. Other requests undergo load, bank/entry, volume
/// policy and type-enable checks before an event is reserved.
///
/// Only the low bytes of `panOffset` and `attenuation` are stored, as signed
/// values. Pan adds three SPU pan steps per unit to each voice's base pan.
/// Attenuation scales its gain index by (127 - magnitude) / 127 for magnitudes
/// 0..127; -128 instead scales it by 1/127. Zero leaves that index unchanged.
///
/// Admission does not guarantee playback: dispatch can still fail to obtain a
/// script slot. Any matching bank descriptor must have a completed, valid
/// script image and initialized sample tables. Those resources must remain
/// loaded until the queued start and resulting script have finished using them.
s32 sndEvtRequestScriptStart(s32 soundId, s32 panOffset, s32 attenuation);

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
