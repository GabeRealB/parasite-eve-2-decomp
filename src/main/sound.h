#ifndef MAIN_PRIVATE_SOUND_H
#define MAIN_PRIVATE_SOUND_H

#include <psyq/sys/types.h>

#include "types.h"

#include "main/sound_types.h"
#include "sound_types.h"

extern SndBank Snd_Banks[];

extern SndLoadState SndLoad_State;

extern volatile u8 D_80082120;

extern s8 Snd_BankSlotsByType[];

extern volatile s32 D_800689E4;

extern volatile s16 D_800689EC;

extern s32 D_80068A78;

extern volatile u8 D_80082121;

extern volatile u8 D_80082122;

extern volatile s32 D_80082124;

extern volatile s32 D_80082128;

extern volatile u8 D_8008212C;

extern volatile s32 D_80082130;

extern volatile s8 D_80082134;

extern volatile u8 D_80082135;

extern volatile u8 D_80082136;

extern void* Snd_SequenceBankBuffer;

void Spu_WaitDma(void);

void Audio_IrqFrameWork(void);

SndBank* Snd_AllocBank(SndBankPayload* payload);

void* SndHeap_Malloc(size_t);

void SndHeap_Free(void* ptr);

void Snd_FreeBank(SndBank* bank);

SndBank* Snd_FindBank(u16 bankId);

void Snd_BuildGroupIndex(SndBank* bank);

void LinInterp_Setup(LinInterp* arg0, s32 arg1, s32 arg2, s32 arg3);

void LinInterp_Step(LinInterp* arg0);

void Spu_ApplyPanVolume(s16* arg0, s16 arg1, s32 arg2);

void AsyncCb_Cancel(s32 arg0);

/// Queues asynchronous callbacks and returns a one-based cancellation handle, or zero when full.
s16 AsyncCb_Enqueue(AsyncCbEntry* callbacks);

s32 Spu_AllocVoice(s16* arg0, s32 arg1, s32 arg2);

void Spu_SetVoiceCallbacks(u32 voiceIdx, SpuVoiceCallback arg1, void* arg2);

s32 Spu_SetVoiceRange(s32 idx, s32 arg1, s32 arg2);

s32 Spu_GetVoiceRef(s8 arg0, SpuVoiceRef* arg1);

u8 Spu_GetVoiceStatus(u32 voiceIdx);

void Spu_ClearVoiceCallbacks(u32 voiceIdx);

void Spu_KeyOn(u32 voiceIdx);

void Spu_ArmKeyOn(u32 voiceIdx);

void Spu_KeyOff(u32 voiceIdx);

u16 Spu_CalcVolume(s32 arg0, s32 arg1, s32 arg2, s32 arg3);

SndBankLayer* Snd_GetNote(SndBank* bank, u8 group, u8 layer);

void Spu_FlushVoiceUpdates(void);

s32 Spu_ReleaseVoiceSlot(u32 voiceIdx);

void Spu_ConfigReverb(s32 mode);

void Spu_SetReverbDepth(s16 depth);

void Spu_EnableReverbVoice(u32 voiceIdx);

void Spu_DisableReverbVoice(u32 voiceIdx);

void SndEvt_Process(void);

/// Takes a free slot from the pool and marks it in use, or returns `NULL` when
/// every slot is already taken.
///
/// The pool is fixed, so a request that finds nothing free fails instead of
/// growing it: an enqueuer either reports that failure to its own caller or
/// drops the command. The slot comes back with `command` set to
/// `SOUND_EVENT_NO_OP` and no arguments written. The caller replaces that
/// command and writes every argument it reads before queueing the slot with
/// `sndEvtEnqueue`.
SndEvt* sndEvtAlloc(void);

s32 Midi_IsChannelFree(u8 arg0);

/// Appends an already-filled-in event to the pending queue, where a later
/// processing pass runs the handler its command selects.
///
/// The queue takes the event as it stands, so the caller writes the arguments
/// first; a `NULL` event is ignored.
void sndEvtEnqueue(SndEvt* event);

void Midi_SetMasterVolume(s32 arg0);

void SndEvt_EnqueueType5Pending(void);

void SndEvt_FlushType5Pending(void);

s32 SndLoad_ResolveSpuAddr(s32 arg0, s32 arg1);

s32 SndLoad_ProcessSector(u32* arg0);

void SndLoad_FromSectorMode8(void* arg0);

void SndLoad_BeginFromBuffer(u8 arg0, void* arg1);

void SndLoad_Teardown(void);

s32 SndLoad_FeedSector(void* arg0);

s32 SndLoad_FeedSectorOrError(void* arg0);

s32 SndBank_FinalizeLoad(SndLoadState* load);

void Snd_SetMutedVolumes(s32 arg0);

void SndVoice_KeyOffMatching(void);

s32 SndVoice_AllocSlot(s32 arg0, s8 arg1, s8 arg2, SndBankSlot* slot, SndScriptEntryControls* entryControls);

s32 SndScript_StopMatching(s32 arg0, s32 arg1);

void SndVoice_FadeMatching(s32 arg0, s32 arg1);

void SndVoice_SetPanRamp(s32 arg0, s32 arg1, s32 arg2);

void SndVoice_SetVolumeRamp(s32 arg0, s32 arg1);

void SndVoice_IncRefCount(void);

void SndVoice_TickRefCount(void);

s32 SndVoice_FindById(s32 arg0);

void SndVoice_ApplyMasterVolume(s8 arg0);

s8 SndVoice_GetMasterVolume(void);

SndBankSlot* SndBankSlot_Get(s32 arg0);

void SndBankSlot_Free(s32 arg0);

void Spu_Init(void);

void AsyncCb_Poll(void);

void AsyncCb_Reset(void);

void Spu_InitVoices(void);

s32 AudioTick_Insert(AudioTickPoll poll, AudioTickOnRemove onRemove, u16 id, s32* arg);

void SndEvt_Reset(void);

s32 Midi_InitSystem(u32);

s32 Midi_Tick(s32* unused);

void Snd_PollAsync(s32 unused);

void Snd_RegisterTickCallbacks(void);

s32 Snd_ReverbWarmupCb(s32* arg0);

s32 Snd_InitBanks(u32);

void Spu_ResetCommonAttr(void);

/// Refreshes hardware voice status and runs pending voice callbacks and updates.
extern void Spu_TickVoices(void);

#endif // MAIN_PRIVATE_SOUND_H
