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

/// Reserves a bank descriptor's program, layer and first-layer tables for a load.
///
/// The high nibble of `payload->bankId` selects the descriptor slot; unsupported
/// types return NULL. Type 4 uses the active upload slot. Non-sequence loads
/// release the slot's prior tables before allocating, so failure can leave it
/// free. Type F reuses retained sequence storage when available, which must be
/// large enough for all three tables. Types 2, E and F reserve minimum capacities.
/// The returned descriptor is borrowed; counts, id and table contents are filled
/// by the loader. NULL also reports allocation failure. `payload` is not retained.
SndBank* sndBankAllocTables(const SndBankPayload* payload);

/// Allocates a 4-byte-aligned payload from the sound heap, or returns NULL.
///
/// `payloadBytes` excludes the private block header. The header and payload are
/// rounded up together; a remainder no larger than a header stays in the block.
/// Zero bytes still reserve a block. The request plus header and alignment must
/// fit in size_t without wrapping. Storage lasts until `sndHeapFree` or a sound
/// system reset. Call with sound-heap operations serialized against audio updates.
void* sndHeapAlloc(size_t payloadBytes);

/// Releases a sound-heap payload and coalesces adjacent free blocks.
///
/// NULL is accepted; otherwise `payload` must be a live result of `sndHeapAlloc`.
/// The numeric range check includes the buffer's end and does not validate an
/// arbitrary pointer. The allocation flag is cleared before either recognized
/// header marker is checked. Call with sound-heap operations serialized.
void sndHeapFree(void* payload);

/// Releases a non-sequence bank's table block and marks its descriptor free.
///
/// NULL and type-F descriptors are left alone, including the free-id sentinel.
/// Groups, layers and first-layer indices become NULL; their borrowed pointers
/// cease to be valid. Separate script/sequence images and SPU samples are not
/// released here. Counts and the SPU base remain stored in the descriptor.
void sndBankFree(SndBank* bank);

/// Finds the first descriptor with the exact bank id, or returns NULL.
///
/// `SOUND_BANK_ID_FREE` requests bank 0 rather than finding an unused slot.
/// The result is a borrowed, stable descriptor; reload/reset replaces its
/// contents. Finding it does not prove that its tables have finished loading.
/// No bank-type-only matching is performed.
SndBank* sndBankFind(u16 bankId);

/// Builds each program's first-layer offset, measured in SndBankLayer elements.
///
/// A NULL `groupFirstLayer` skips the build. Otherwise it must hold at least
/// max(1, groupCount) writable u16 entries: entry zero is always written, even
/// for zero groups. Nonempty banks require groupCount readable program records;
/// each following index sums the preceding programs' layer counts.
void sndBankBuildLayerIndex(SndBank* bank);

/// Initializes a normalized fade direction from two low-byte level selectors.
///
/// Only the selectors' ordering matters: increasing selects 0 -> 65535,
/// decreasing selects 65535 -> 0. Equal selectors or zero updateCount bypass
/// scaling and clear the gains and step, leaving direction unchanged.
/// The signed step is 65535 / updateCount, truncated toward zero. Positive
/// counts 1..65535 advance the fade; larger counts hold it, and negative counts
/// retain the signed quotient and the step routine's unsigned gain arithmetic.
/// Each caller determines what one update means; rounding can extend a fade.
void linInterpSetup(LinInterp* ramp, s32 startLevel, s32 endLevel, s32 updateCount);

/// Advances a ramp by one step and clamps its gain toward its selected endpoint.
///
/// A zero step holds the gain; enabled does not gate stepping. Signed step
/// participates in unsigned gain arithmetic. Reaching the endpoint leaves the
/// step stored until `linInterpApply` clears it. The caller supplies the clock.
void linInterpStep(LinInterp* ramp);

/// Calculates left/right direct SPU volumes for the current output selection.
///
/// `pan` is signed: <=1 is fully left, 64 is centre, >=127 is fully right.
/// Q12 table gains scale a nonnegative `volume` in SPU volume units; each output
/// saturates at 16383. Mono sums the stereo gains and applies the centre gain
/// before writing equal channels. Products retain their 32-bit arithmetic.
/// `volumes` is caller-owned output; no SPU attribute is submitted here.
void spuCalcPanVolumes(SpuVolume* volumes, s16 pan, s32 volume);

/// Returns the resident output selection (0 mono, 1 stereo).
///
/// This is the selection used for voice panning and CD/stream routing, not
/// the saved options-menu value, whose mono/stereo encoding is reversed.
u8 sndOutputIsStereo(void);

/// Cancels the queued job identified by a one-based slot handle.
///
/// Zero does nothing. A nonzero handle must be 1..4 and still identify the
/// caller's pending job: handles have no generation counter. The job stops
/// normal polling immediately; the queue retires it in order, invoking its
/// cancel callback only if polling had started. Its done callback is skipped.
void asyncCbCancel(s16 handle);

/// Queues a job's three handlers and returns its one-based cancellation handle.
///
/// The four-slot ring holds at most three jobs; zero means full. Only pollFn,
/// doneFn and cancelFn are copied, so the caller's entry may be temporary.
/// pollFn must be non-NULL; the other handlers may be NULL. Callbacks receive
/// the queue-owned entry, initialized with firstPoll set and pollState zero.
s16 asyncCbEnqueue(const AsyncCbEntry* callbacks);

/// Takes a hardware voice from an ordered list of registered voice-range indices.
///
/// rangeCount counts readable s16 indices into the four registered ranges;
/// each range must stay within voices 0..23. Nonpositive counts return -1.
/// priority is a nonnegative allocation priority. A free voice whose cached
/// key status is SPU_OFF or SPU_ON_ENV_OFF is returned immediately; otherwise
/// the scan considers held voices at or below the requested priority. Returns
/// the selected voice index, or -1 when no candidate is found.
///
/// Reassignment notifies the previous owner when both callback and context
/// are non-NULL. It retains that registration and any pending hardware changes;
/// the new owner must replace or clear the callback before the next audio tick.
s32 spuAllocVoice(const s16* rangeIndices, s32 rangeCount, s32 priority);

/// Registers one owner's release notification and borrowed context for a voice.
///
/// voiceIdx must be 0..23. Notification on reassignment or detected completion
/// requires both callback and context to be non-NULL; either NULL disables it.
/// The context must remain live until the registration is replaced or cleared.
void spuSetVoiceCallback(u32 voiceIdx, SpuVoiceCallback callback, void* context);

s32 Spu_SetVoiceRange(s32 idx, s32 arg1, s32 arg2);

/// Gets a voice's attributes in the batch waiting for the next SPU flush.
///
/// voiceIdx must be 0..23 and ref must be writable. Returns 1 for an existing
/// entry, preserving its edits, or 0 after appending one with an empty mask.
/// Only a new entry clears ref's three unknown bytes. Select changed attributes
/// with SPU_VOICE_* mask bits; ref->attr is valid only until the next flush.
s32 spuGetVoiceRef(s8 voiceIdx, SpuVoiceRef* ref);

/// Returns a voice's key status sampled at the last audio tick.
///
/// voiceIdx must be 0..23. Values are SPU_OFF, SPU_ON, SPU_OFF_ENV_ON and
/// SPU_ON_ENV_OFF; this reads cached state without querying the hardware.
u8 spuGetVoiceKeyStatus(u32 voiceIdx);

/// Removes a voice's release callback and context without releasing its slot.
///
/// voiceIdx must be 0..23. Key state and queued hardware updates are untouched.
void spuClearVoiceCallback(u32 voiceIdx);

/// Queues a sound voice's key-on for the next SPU flush.
///
/// voiceIdx must be 0..23. Restarts the five-tick completion grace period,
/// cancels a pending key-off and removes the silent-block mark. The flush
/// records this voice for reset onto the silent block when playback ends.
void spuKeyOn(u32 voiceIdx);

void Spu_ArmKeyOn(u32 voiceIdx);

/// Queues a voice's key-off and withdraws both kinds of pending key-on.
///
/// voiceIdx must be 0..23. Applied at the next SPU flush; allocation and the
/// owner's callback registration remain until separately released or completed.
/// The retained no-voice byte 0xFF selects bit 31 on the MIPS target rather
/// than one of the 24 hardware voices.
void spuKeyOff(u32 voiceIdx);

/// Converts a key and tuning offset to a 14-bit SPU pitch register value.
///
/// key and rootKey are semitones; pitchOffset is signed 1/256-semitone units,
/// and fineTune is added in 1/128-semitone units. The lookup coordinate is
/// pitchOffset + (key - rootKey) * 256 + fineTune * 2 + 72 * 256, wrapped to
/// 16 bits. Its high byte must index the 96-entry semitone table (0..95);
/// the fractional lookup discards the low bit. No bounds check is performed.
/// The low half of the scaled product saturates at 0x3FFF; 0x1000 is unity pitch.
u16 spuCalcPitch(s32 key, s32 pitchOffset, s32 rootKey, s32 fineTune);

/// Returns a bank-owned sample layer selected by program and layer indices.
///
/// A NULL bank returns NULL. Otherwise the bank must be fully loaded,
/// group < bank->groupCount and layer < bank->groups[group].layerCount.
/// The unchecked lookup returns storage valid until the bank is released
/// or reloaded; callers do not own it.
SndBankLayer* sndBankGetLayer(SndBank* bank, u8 group, u8 layer);

void Spu_FlushVoiceUpdates(void);

/// Returns a voice's allocation slot to the pool without changing hardware state.
///
/// voiceIdx must be 0..23. Clears allocation, priority and age, retaining the
/// callback, key status and pending updates. Returns 0 on release and -1 when
/// the narrowed index fails the retained guard, which also admits index 24.
s32 spuReleaseVoiceSlot(u32 voiceIdx);

void Spu_ConfigReverb(s32 mode);

void Spu_SetReverbDepth(s16 depth);

/// Queues reverb routing on for one voice at the next SPU flush.
///
/// voiceIdx must be 0..23. Withdraws a pending disable for this voice;
/// this request does not change the cached hardware reverb status.
void spuEnableVoiceReverb(u32 voiceIdx);

/// Queues reverb routing off for one voice at the next SPU flush.
///
/// voiceIdx must be 0..23. Withdraws a pending enable for this voice;
/// this request does not change the cached hardware reverb status.
void spuDisableVoiceReverb(u32 voiceIdx);

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

/// Returns one when the volume policy permits selecting a different sequence.
///
/// Reduced mode exactly equal to one rejects every selection. Otherwise 255
/// is eligible, and other ids are eligible only when they differ from the
/// resident song's loaded id. Playback phase and voice availability are ignored.
s32 midiCanSelectSequence(u8 sequenceId);

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

/// Enables MIDI music muting and requests zero gain for all sequences.
///
/// The output gate takes effect on the next voice-volume refresh and remains
/// enabled even if the event pool is full. A queued request refreshes every
/// channel and replaces the last-requested gain with zero; the previous gain
/// is not saved. Sequence 0x5A ignores both the gate and the requested gain.
/// The resident sequence must have a loaded id in 0..99 when the queued volume
/// request is processed. `midiUnmuteMusic` clears the gate.
void midiMuteMusic(void);

/// Releases MIDI music muting and requests the last queued gain for all sequences.
///
/// Does nothing when the output gate is already clear. Otherwise the gate is
/// cleared before reserving an event, and stays clear even if the pool is full.
/// The cached gain is the latest request, including zero queued by muting;
/// no earlier gain is saved. A byte with bit 7 set requests full gain (127).
/// A queued request refreshes every channel on processing; gate release alone
/// does not mark channels for refresh. The resident sequence must have a loaded
/// id in 0..99 when processed. Sequence 0x5A ignores the gate and requested gain.
void midiUnmuteMusic(void);

/// Selects an SPU byte origin for a script bank's sample pool and updates placement state.
///
/// Bits 12..15 of `bankId` select the bank type; other bits are ignored.
/// `waveBytes` is a nonnegative sample-pool byte count, rounded up to 64 bytes
/// without signed overflow. Fixed-base types are 0, 3 and 14; types 1, 2, 5,
/// 6 and 7 place below a region boundary. Type 5 uses the current type-1 origin
/// when present. Type 4 starts at its region base or appends after the preceding
/// loaded descriptor, advancing the upload ordinal; it admits three banks per
/// batch unless the pending mode restarts it. Their descriptors must remain live.
/// Appending uses the preceding descriptor's unrounded sample length; the end
/// marker uses the current rounded length. The caller must ensure the sample
/// pool fits its assigned SPU region; no free-space validation is performed.
/// Unsupported types and an exhausted type-4 batch return zero; the latter also
/// clears its placement-end marker. No sample data is transferred or freed.
s32 sndLoadPlaceScriptSamples(s32 waveBytes, s32 bankId);

/// Feeder failure result, distinct from the loader's nonnegative phase values.
enum { SOUND_LOAD_RESULT_ERROR = -1 };

/// Consumes one aligned sound-load payload and returns its new `SOUND_LOAD_PHASE_` value.
///
/// Uses the resident load's `sectorBytes`, phase and upload policy. The header
/// payload must contain the five-word header and all declared group/layer tables;
/// image payloads contain up to `sectorBytes` bytes, and sample payloads contain
/// the lesser of that count and the remaining wave length. All lengths must fit
/// their buffers and the assigned SPU region. The sequence image must fit its
/// resident 10 KiB buffer. No stream bounds are checked. Image lengths retain
/// 16-bit word-alignment truncation; an exactly full final image sector needs
/// one more feed to enter the sample phase. Failed early loads drain through
/// the header's transfer-sector count, retaining the byte arrival counter.
/// Polling uploads require the previous DMA to be complete. This call borrows
/// `payloadWords` through the copy or DMA; retain sample bytes until DMA finishes.
/// Installation is separate; CD-audio feeds start directly in the upload phase.
s32 sndLoadProcessSector(u32* payloadWords);

/// Starts a whole-sector sound-bank load using a borrowed CD destination buffer.
///
/// `sectorBuffer` must remain live, word-aligned and writable for 2048-byte CD
/// reads until streaming completes or is cancelled. No sector is consumed here.
/// The caller must finish or tear down the previous load first: resetting drops
/// its image and bank pointers without releasing them. Upload policy and saved
/// bank-selection state are retained; the CD-ready feeder selects polling.
void sndLoadBeginSectorLoad(void* sectorBuffer);

/// Starts a file-chunk sound-bank load and saves the current bank-selection state.
///
/// `sectorBuffer` is borrowed for the load; subsequent sectors are supplied by
/// the feeder, and this call consumes none. Feeds must provide word-aligned
/// 2048-byte sectors. With `syncUpload` zero, the feeder skips a 16-byte prefix
/// at each section start and polls the SPU DMA; nonzero uses whole sectors and
/// waits for each DMA. The byte is retained without normalization. The caller
/// must finish or tear down the previous load before its pointers are reset.
void sndLoadBeginChunkLoad(u8 syncUpload, void* sectorBuffer);

/// Restores the saved character-bank selection and releases an interrupted load.
///
/// Marks the resident load torn down and releases its held image and bank,
/// except when an early failure is still draining its sectors. The published
/// load-bank id is retained. The caller handles CD delivery cancellation and
/// retains any sample source buffer until DMA ends. Successful installation
/// has already detached its resources from the load.
void sndLoadTeardown(void);

/// Feeds a file-chunk sector and installs a completed sequence or script bank.
///
/// `sector` supplies a word-aligned 2048-byte buffer. Polling uploads omit its
/// first 16 bytes at each header, image or sample section start; synchronous
/// uploads consume whole sectors. Returns the new `SOUND_LOAD_PHASE_` value,
/// `SOUND_LOAD_RESULT_ERROR` while bank initialization is busy or on a hard
/// transfer failure, or zero for a torn-down polling load. Early failures drain
/// to DONE without installation. The installation result is deliberately ignored.
/// Sample bytes must remain live until DMA completes; other bytes are copied.
/// Stop feeding at DONE, including in synchronous mode.
s32 sndLoadFeedChunkSector(u8* sector);

/// Feeds a whole-sector stream payload without installing the finished image.
///
/// `payload` is word-aligned and sized for the resident load's `sectorBytes`;
/// sample bytes remain live until DMA completes. Returns its new phase, mapping
/// the hard ERROR phase to `SOUND_LOAD_RESULT_ERROR`. The CD-ready caller stops
/// delivery and installs its sequence after DONE; no section prefix is skipped.
s32 sndLoadFeedSector(u32* payload);

/// Binds a finished sequence load to the resident song and detaches its resources.
///
/// Call once after the final sample payload is submitted. Sample DMA must finish
/// before playback or source-buffer reuse. The bank and layer table must be live,
/// with sample offsets still relative to its SPU base. Uses the bank id's low byte as the sequence id,
/// rebases layer addresses and builds program indices; the song borrows its image
/// until that resident buffer is reused. Clears the published load id and returns
/// zero on success or `SOUND_LOAD_RESULT_ERROR` for loader failure/a free bank id.
/// Failure retains the load's resource pointers for the caller to resolve.
s32 sndLoadInstallSequence(SndLoadState* load);

void Snd_SetMutedVolumes(s32 arg0);

void SndVoice_KeyOffMatching(void);

s32 SndVoice_AllocSlot(s32 arg0, s8 arg1, s8 arg2, SndBankSlot* slot, SndScriptEntryControls* entryControls);

s32 SndScript_StopMatching(s32 arg0, s32 arg1);

/// Starts a mute or unmute ramp on every matching sound-script instance.
///
/// `soundSelector` is an exact resolved sound id, or a type-only word in bits
/// 28..31; bank or instance wildcards are not supported. No remapping occurs
/// here. Nonzero `muted` mutes running or releasing slots; zero unmutes only
/// slots in the muting state. Other states are unchanged. Each accepted change
/// restarts a normalized ramp with an eight-audio-update duration request,
/// which truncation can extend. Command execution pauses during mute/unmute;
/// existing voices continue ticking. SPU volume changes occur on audio updates.
void sndScriptSetMuteMatching(s32 soundSelector, s32 muted);

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

/// Returns the first live script-slot index with the exact resolved sound id, or -1.
///
/// Slots are scanned in ascending order and the result is 0..7. Starting,
/// running, releasing and fading-out slots qualify; idle, stopping, muting and
/// unmuting slots do not. No bank remapping or resource-readiness check occurs.
s32 sndScriptFindInstanceById(s32 soundId);

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

/// Releases a sound-script bank slot's owned image while retaining its descriptor.
///
/// Only the low byte of `slotIndex` is used; values 0..15 select a stable slot,
/// and all others do nothing. Pending events, scripts and borrowed chunks must
/// have finished using the image before release. NULL images are accepted.
/// The image becomes NULL and its cached id becomes -1; descriptor tables and
/// the SPU origin remain intact and require their separate cleanup.
void sndBankSlotReleaseImage(s32 slotIndex);

void Spu_Init(void);

void AsyncCb_Poll(void);

void AsyncCb_Reset(void);

void Spu_InitVoices(void);

/// Results of registering an audio-update poll.
enum {
    AUDIO_TICK_INSERTED     = 0,
    AUDIO_TICK_NO_MEMORY    = -1,
    AUDIO_TICK_DUPLICATE_ID = -2
};

/// Driver registration ids; their ordering runs MIDI, scripts, then reverb warmup.
enum {
    AUDIO_TICK_ID_MIDI          = 0x4800,
    AUDIO_TICK_ID_SOUND_SCRIPTS = 0x8800,
    AUDIO_TICK_ID_REVERB_WARMUP = 0x8801
};

/// Registers an audio-update poll under a unique ascending unsigned 16-bit id.
///
/// NULL poll and removal handlers are accepted. `pollArg` is passed unchanged
/// to each poll and must outlive the registration; the driver neither reads nor
/// frees it. A poll returning -1 ends registration and invokes onRemove first.
/// Removed nodes remain allocated until a sound-system reset. An audio update
/// arriving while the list is disabled skips all polls. This guard does not
/// serialize concurrent list edits; callers must serialize registrations.
/// Returns AUDIO_TICK_INSERTED, AUDIO_TICK_NO_MEMORY or AUDIO_TICK_DUPLICATE_ID.
s32 audioTickInsert(AudioTickPoll poll, AudioTickOnRemove onRemove, u16 id, s32* pollArg);

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
