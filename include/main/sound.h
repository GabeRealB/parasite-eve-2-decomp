#ifndef MAIN_SOUND_H
#define MAIN_SOUND_H

#include "types.h"

#include "main/sound_ids.h"

#include "main/sound_types.h"

extern s32 D_800820E0;

extern s16 D_800820E4;

extern s16 D_800820E6;

/// Selects the saved music-volume option instead of an explicit gain override.
enum { MIDI_MUSIC_VOLUME_SAVED = 0 };

/// Applies a music gain override or the saved music-volume option, and saved output mode.
///
/// `volumeOverride` is a 16-bit level: zero (`MIDI_MUSIC_VOLUME_SAVED`)
/// selects the live save's musicVolume option, which must be in 0..3 and maps
/// to gains 100, 64, 32, 0. Otherwise only its low byte is sent: 0..127 is
/// sequence gain (127 full), and a byte with bit 7 set requests 127.
/// A nonzero override with a zero low byte requests silence. The level is
/// cached as a signed halfword for event-script fades, even if the gain request
/// fails. An explicit override leaves the music mute gate alone.
///
/// Saved option 3 enables the mute gate; other options release it. This happens
/// before queueing the selected gain, after any gain request made by that gate
/// change. The room song is selected when nonzero; zero addresses all sequences.
/// Requests are deferred and may be dropped when the event pool is full;
/// sequence 0x5A ignores the requested gain and gate. The matching loaded sequence
/// must have an id in 0..99 when the request is processed.
///
/// Every call also applies the live save's soundMode (0 stereo, otherwise mono)
/// to MIDI, sound scripts, streams and CD input.
void midiApplyMusicVolume(u16 volumeOverride);

/// Resident output selections; the setter consumes only the stereo bit.
enum {
    SOUND_OUTPUT_MONO   = 0,
    SOUND_OUTPUT_STEREO = 1
};

/// Selects mono or stereo output for MIDI, sound scripts, streams and CD input.
///
/// Only bit zero of `enabled` matters: 0 selects mono and 1 selects stereo.
/// Existing MIDI channels and eligible script voices are marked for remixing
/// on their next volume update without changing their master gains. Streaming
/// voices use the selection on setup or their next gain update. CD-input
/// attenuation is applied immediately: mono uses four gains of 90; stereo
/// uses gains 120, 0, 120, 0 in `CdlATV` order.
void sndOutputSetStereo(s32 enabled);

/// Applies a ramp's normalized gain to a playback level, or bypasses scaling.
///
/// With LINEAR_INTERPOLATOR_SCALE, the unsigned 32-bit product of level and gain
/// is divided by 65535; callers use nonnegative audio levels whose product fits.
/// At the endpoint this also clears the step, retaining the gain and scaling.
/// Other enabled values return level unchanged. The ramp remains caller-owned.
s32 linInterpApply(LinInterp* ramp, s32 level);

/// Admission failures shared by MIDI start and stop requests.
enum {
    SOUND_EVENT_MIDI_REQUEST_POOL_FULL        = -2,
    SOUND_EVENT_MIDI_REQUEST_INVALID_SEQUENCE = -3
};

/// Queues a sequence start with an optional fade-in and returns zero, or an admission failure.
///
/// The low byte of `sequenceId` selects an exact loaded id, including zero;
/// 255 is rejected. Only the low halfword of `fadeTicks` is copied, in audio
/// updates including extra PAL timer updates. Zero bypasses interpolation;
/// other values request gain steps of 65535 / fadeTicks, with rounding that
/// can extend the fade. A zero master gain also bypasses interpolation.
/// Dispatch starts only an idle, matching song with a valid loaded image.
/// Acceptance does not guarantee playback. A full pool drops the request;
/// both arguments are copied and no caller storage is retained.
s32 sndEvtRequestMidiStart(s32 sequenceId, s32 fadeTicks);

/// Queues a sequence stop with an optional fade-out and returns zero, or an admission failure.
///
/// The low byte of `sequenceSelector` selects a loaded id; zero selects every
/// sequence and 255 is rejected. `fadeTicks` is truncated to 16 bits and rounded
/// down to a multiple of four audio updates, including extra PAL timer updates.
/// A normally playing song fades with gain steps of 65535 / fadeTicks before
/// stopping; other phases enter stopping immediately. Zero duration or zero
/// master gain bypasses interpolation. Rounding can extend a nonzero fade.
/// A selector with no match is ignored at dispatch. A full pool drops the
/// request; both arguments are copied and no caller storage is retained.
s32 sndEvtRequestMidiStop(s32 sequenceSelector, s32 fadeTicks);

/// Returns one when a matching sequence is playing, muted, fading in or fading out.
///
/// Only the low byte of `sequenceSelector` matters: zero selects every sequence,
/// and 255 always returns zero. Idle, stopping and unmuting phases return zero.
/// This query reads the current playback phase; queued commands are not included.
s32 midiIsSequenceBusy(s32 sequenceSelector);

/// Returns the resident MIDI master gain (0 silent, 127 full).
///
/// The stored byte is zero-extended to `s32` and scales sequence gain by 1/127
/// before the song's volume ramp is applied.
s32 midiGetMasterVolume(void);

/// Admission failures returned by `sndEvtRequestMidiVolume`.
enum {
    SOUND_EVENT_MIDI_VOLUME_POOL_FULL        = -2,
    SOUND_EVENT_MIDI_VOLUME_INVALID_SEQUENCE = -3
};

/// Queues a sequence gain change and returns zero, or an admission failure.
///
/// Only the low byte of `sequenceSelector` matters: zero selects all sequences,
/// other ids select the matching loaded sequence, and 255 is rejected before
/// reserving a slot. Only the low byte of `volumeScale` matters: 0..127 is the
/// requested gain (0 silent, 127 full); a byte with bit 7 set requests 127.
/// A full event pool drops the request and leaves the last-requested gain intact.
///
/// Dispatch multiplies the sequence's mix-table level by this gain and refreshes
/// every channel; master gain and the fade ramp apply separately. The matching
/// sequence must have a loaded id in 0..99 when processed, including for selector
/// zero. Sequence 0x5A ignores this requested gain. Queueing updates the shared
/// last-requested gain even if the selector matches no sequence; it does not
/// release the music mute gate. Both arguments are copied into the event.
s32 sndEvtRequestMidiVolume(s32 sequenceSelector, s32 volumeScale);

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

/// Sets admission gates for sound-script starts, mute/unmute and mix requests.
///
/// The low bit of `enabled` is stored (0 closed, 1 open). `typeSelector`'s top
/// nibble selects one of sixteen bank types; the remaining bits are ignored.
/// `SOUND_BANK_TYPE_ALL_NON_AMBIENT` selects all sixteen gates here, including
/// ambient. Script starts whose entry permits disabled types bypass this gate.
/// Selecting only the character type with raw `enabled == 0` also queues a stop
/// retaining ADSR release; a full event pool drops only that stop. Nonzero inputs
/// with a clear low bit close gates without queuing it. Other changes do not stop
/// existing instances or cancel requests already queued.
void sndScriptSetTypeRequestsEnabled(s32 enabled, s32 typeSelector);

/// Returns 1 when a qualifying script instance owns the requested id, otherwise 0.
///
/// Type-1 ids resolve against the currently loaded type-1 bank; if a matching
/// descriptor exists, its completed image must be live during this call.
/// The resolved id is matched exactly against starting, running, releasing or
/// fading-out slots. Muting, unmuting and stopping slots are excluded. A match
/// does not require an attached SPU voice and does not prove audibility.
s32 sndScriptHasActiveId(s32 soundId);

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
