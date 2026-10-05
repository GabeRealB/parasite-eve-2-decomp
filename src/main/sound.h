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

/// Value of `gSndLoadBankId` when no sound-bank load has published its id.
///
/// The stored word is 255. Readers compare that whole word before applying
/// `SOUND_BANK_TYPE_MASK`, so a bank id that only ends in 255 does not match.
/// A free descriptor uses `SOUND_BANK_ID_FREE`.
enum { SOUND_LOAD_BANK_NONE = 0xFF };

/// 16-bit bank id of a sound-bank load that has accepted its header, or
/// `SOUND_LOAD_BANK_NONE` when none has.
///
/// The id is published before the previous bank is released. The sentinel is
/// stored again when setup, completion or finalization finishes the load.
/// Until then, a script-start request whose bank type matches the type of
/// this id is refused. Sector feeding and those requests share the word, so
/// each access is a volatile 32-bit load or store of the zero-extended id.
/// Stopping the transfer any other way leaves the published id in place.
extern volatile s32 gSndLoadBankId;

// States of the shared MIDI/script volume policy; the stored flag is a halfword.
enum {
    /// Ordinary MIDI/script volume policy without reduced-mode request restrictions.
    ///
    /// The initial value of `gSndVolumeReducedMode`. MIDI selection and flagged
    /// script starts still undergo their other eligibility checks.
    /// `Snd_SetMutedVolumes(0)` stores this value before applying MIDI master
    /// volume 64 and script master volume 127/127. Independent gain changes
    /// leave the policy value unchanged.
    SOUND_VOLUME_MODE_NORMAL  = 0,
    SOUND_VOLUME_MODE_REDUCED = 1
};

/// Reduced-volume policy shared by MIDI selection and sound-script requests.
///
/// Starts in `SOUND_VOLUME_MODE_NORMAL` (0). Entering
/// `SOUND_VOLUME_MODE_REDUCED` (1) applies MIDI master volume 0 and script
/// master volume 40/127; restoring normal mode applies 64 and 127/127.
/// Reduced mode makes the sequence-selection eligibility query return false
/// and refuses script starts marked `SOUND_SCRIPT_REJECT_IN_REDUCED_VOLUME_MODE`.
/// Demo entry enables it; main-loop initialization restores normal mode.
/// The mode is stored before the master-volume updates. Other volume setters
/// do not change it, so it records the policy rather than the current gains.
extern volatile s16 gSndVolumeReducedMode;

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

void LinInterp_Setup(LinInterp* ramp, s32 arg1, s32 arg2, s32 arg3);

void LinInterp_Step(LinInterp* ramp);

void Spu_ApplyPanVolume(s16* arg0, s16 arg1, s32 arg2);

void AsyncCb_Cancel(s32 arg0);

/// Queues asynchronous callbacks and returns a one-based cancellation handle, or zero when full.
s16 AsyncCb_Enqueue(AsyncCbEntry* callbacks);

s32 Spu_AllocVoice(s16* arg0, s32 arg1, s32 arg2);

void Spu_SetVoiceCallbacks(u32 voiceIdx, SpuVoiceCallback callback, void* context);

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

/// Reserves the first free sound-event slot, or returns `NULL` when every slot
/// is already reserved.
///
/// The pool is fixed, so a request that finds nothing free fails instead of
/// growing it. Producers either report that failure or drop the command. The
/// slot is marked reserved and `command` is `SOUND_EVENT_NO_OP`; previous
/// argument bytes stay. The caller replaces `command`, writes every argument
/// that command reads, and passes the slot once to `sndEvtEnqueue`. A
/// reservation that is never queued stays occupied until reset or invalid-command
/// recovery clears the pool.
SndEvt* sndEvtAlloc(void);

s32 Midi_IsChannelFree(u8 arg0);

/// Appends one reserved sound-event slot to the deferred-command FIFO.
///
/// The caller sets `command` and every argument that command reads, then passes
/// that slot once. The slot is linked in place. When nothing is queued it
/// becomes both head and tail and `prev` is cleared; otherwise `prev` records
/// the previous tail and that tail's `next` points at the slot. `next` is
/// cleared either way.
/// `allocated`, `command` and `args` are not written. A `NULL` argument is
/// ignored and does not change the drain gate. Otherwise the audio interrupt's
/// drain stays closed while those links are stored, then reopens. This call
/// does not run the command handler.
void sndEvtEnqueue(SndEvt* event);

/// Sets the resident MIDI master gain and schedules every channel's volume refresh.
///
/// Only the low byte of `volume` matters: 0..127 stores that gain, and a byte
/// with bit 7 set stores full gain (127). Existing voices receive the new gain
/// on the next volume update; this also refreshes mix-mode dependent pan.
void midiSetMasterVolume(s32 volume);

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

/// Updates a sound-script instance's pan and attenuation, ramping larger changes.
///
/// The low three bits of `scriptSlotIndex` select one of eight resident slots;
/// the caller must select a live instance. `panOffset` is a signed-byte offset
/// adding three SPU pan steps per unit. Its delta wraps to a signed byte:
/// magnitudes through 8 snap, larger changes step by 2 offset units per voice
/// visit. Only the target's low byte is stored.
///
/// The low signed byte of `attenuation` requests a volume-table index scale of
/// (127 - magnitude) / 127 for magnitudes 0..127. It uses
/// `sndScriptRampVolume`'s attenuation ramp;
/// -128 instead targets zero attenuation. Both ramps advance once per voice
/// visited during audio updates, and hardware mixing is deferred to that update.
void sndScriptRampMix(s32 scriptSlotIndex, s32 panOffset, s32 attenuation);

/// Updates a sound-script instance's volume scale, ramping larger attenuation changes.
///
/// The low three bits of `scriptSlotIndex` select a live resident instance.
/// Only the low seven bits of `volumeScale` matter (0 silent, 127 full): their
/// complement is the target attenuation, compared with the current signed byte.
/// Differences through 32 snap; larger changes step by 8 attenuation units per
/// voice visited during audio updates. Hardware mixing is deferred to that update.
void sndScriptRampVolume(s32 scriptSlotIndex, s32 volumeScale);

void SndVoice_IncRefCount(void);

void SndVoice_TickRefCount(void);

s32 SndVoice_FindById(s32 arg0);

/// Sets the sound-script master gain and schedules eligible instances for remixing.
///
/// `masterVolume` is normally 0..127 (0 silent, 127 full). Recalculates each
/// eligible voice's gain as master * entry gain * base gain / 127^2; entries
/// exempt from active ducking keep their gain. Hardware volume and pan are
/// refreshed on the next voice visit, including mix-mode changes. The saved
/// master level clamps negative inputs to zero after applying the signed input
/// to existing voices. Live voices require their bank's entry controls to remain loaded.
void sndScriptSetMasterVolume(s8 masterVolume);

/// Returns the current sound-script master gain (0 silent, 127 full).
///
/// During ducking this is the reduced gain; exempt entries use the saved
/// unducked level separately. The result is a signed byte, always nonnegative.
s8 sndScriptGetMasterVolume(void);

/// Returns the stable sound-script bank slot selected by the low byte of `slotIndex`.
///
/// Low bytes 0..15 select a slot; all others return `NULL`. Higher bits are
/// ignored. The borrowed record survives releases and reloads, so finding it
/// does not guarantee a completed image or initialized sample tables: it can
/// hold a boot reservation or a released image. Callers accessing those
/// resources must keep their contents loaded through their last use.
SndBankSlot* sndBankSlotGet(s32 slotIndex);

void SndBankSlot_Free(s32 arg0);

void Spu_Init(void);

void AsyncCb_Poll(void);

void AsyncCb_Reset(void);

void Spu_InitVoices(void);

s32 AudioTick_Insert(AudioTickPoll poll, AudioTickOnRemove onRemove, u16 id, s32* arg);

/// Discards all sound-event reservations and commands, then enables queue draining.
///
/// Clears the complete pool, including payloads and unqueued reservations,
/// and empties both queue endpoints. Previously held slots cease to be reserved.
/// Call with event producers and the audio drain quiescent.
void sndEvtReset(void);

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
