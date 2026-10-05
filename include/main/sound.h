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

/// Returns the resident MIDI master gain (0 silent, 127 full).
///
/// The stored byte is zero-extended to `s32` and scales sequence gain by 1/127
/// before the song's volume ramp is applied.
s32 midiGetMasterVolume(void);

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

/// Queues a deferred pan and attenuation change for one existing sound-script instance.
///
/// `soundId` uses the bank, instance and entry encoding of
/// `sndEvtRequestScriptStart`. The requested bank type must be enabled;
/// disabled types and a full event pool silently drop the request. Type-1 ids
/// select the loaded type-1 bank at queue time, requiring a completed image in
/// any matching bank slot during this call. Dispatch changes only the first
/// starting, running, releasing or fading-out slot with that exact resolved id;
/// a request with no match is ignored. Muting and unmuting slots are excluded
/// from that lookup.
///
/// Only the low signed bytes of `panOffset` and `attenuation` are stored.
/// Pan adds three SPU pan steps per unit to each voice's base pan. Its delta
/// wraps to a signed byte: magnitudes through 8 snap, larger changes ramp by
/// 2 offset units per voice visit. Attenuation requests a gain-index scale of
/// (127 - magnitude) / 127 for magnitudes 0..127; -128 requests the unattenuated
/// level, unlike a start request. Attenuation differences through 32 snap,
/// larger changes ramp by 8 units per voice visit. Hardware mixing and both
/// ramps advance during audio updates, including extra PAL updates.
void sndEvtRequestScriptMix(s32 soundId, s32 panOffset, s32 attenuation);

void SndEvt_EnqueueTypeB(s32 arg0, s32 arg1);

void SndBank_SetEnableFlags(s32 arg0, s32 arg1);

s32 SndVoice_HasActiveId(s32 arg0);

void SndEvt_EnqueueTypeD(void);

void SndEvt_EnqueueTypeE(void);

/// Stop controls that do not request a fade for a running sound-script entry.
enum {
    SOUND_SCRIPT_STOP_NO_FADE      = 0, // Stop with the stored release policy; normally SPU release rate 5
    SOUND_SCRIPT_STOP_KEEP_RELEASE = 1  // Keep the voices' existing ADSR release settings
};

/// OR into a sound id with a nonzero entry to stop every instance, retaining its bank and entry bytes.
enum { SOUND_SCRIPT_STOP_ALL_INSTANCES = 0xFF00 };

/// Queues a deferred stop for matching sound-script instances.
///
/// `soundSelector` uses the bank, instance and entry encoding of a script-start
/// id. A nonzero entry byte selects that exact id; an instance byte of 255
/// selects every instance of the entry. A zero entry byte compares the entire
/// resolved selector with the bank-type nibble, so use a type-only selector
/// or `SOUND_BANK_TYPE_ALL_NON_AMBIENT` to stop all types except ambient type 6.
/// Selector 0 stops common-bank scripts; start-request no-op values are not
/// special here. Type-1 selectors are remapped to the loaded bank at queue
/// time, requiring a valid image in any matching bank slot during this call.
/// A remapped type-only selector contains bank bits and no longer matches
/// a type stop.
///
/// For a running entry, `SOUND_SCRIPT_STOP_NO_FADE` stops without a fade,
/// retaining any earlier keep-release request; otherwise release uses SPU
/// rate 5. `SOUND_SCRIPT_STOP_KEEP_RELEASE` stops without a fade and keeps
/// the voices' ADSR release settings. Controls 2..65535 fade before stopping,
/// with gain steps of 65535 / stopControl per audio update, including extra
/// PAL updates; integer rounding can extend the fade. A zero master level
/// bypasses the gain ramp. Bank-type stops ignore fade durations and replace
/// the release policy: only control 1 keeps the ADSR settings.
///
/// Entry selectors cancel a pending start and stop muted or unmuting scripts
/// without changing their release policy or fading. They leave scripts already
/// fading or releasing alone. The selector and control are copied into the
/// event; no caller storage is retained. A full event pool silently drops the
/// request. Stops do not check bank loading or the bank-type enable gate.
void sndEvtRequestScriptStop(s32 soundSelector, u16 stopControl);

void SndEvt_EnqueueType8(s32 arg0);

void SndEvt_EnqueueType9(s32 arg0);

void Snd_SetModeFlag(s32 arg0);

#endif // MAIN_SOUND_H
