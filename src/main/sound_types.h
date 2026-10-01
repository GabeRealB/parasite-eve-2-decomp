#ifndef MAIN_PRIVATE_SOUND_TYPES_H
#define MAIN_PRIVATE_SOUND_TYPES_H

#include <psyq/sys/types.h>
#include <psyq/libspu.h>

#include "common.h"

#include "main/sound_types.h"

/// Per-frame audio callback; return -1 to remove the registration.
typedef s32 (*AudioTickPoll)(s32* arg);

typedef void (*AudioTickOnRemove)(void);

/// Per-voice SPU runtime (Spu_VoiceState). 24 voices.
typedef void (*SpuVoiceCallback)(void* context);

/// A sound-bank program's layer count and shared volume and pan controls.
///
/// Records are indexed by program number, below the bank's `groupCount`.
/// Each describes a consecutive run of `SndBankLayer` layers; their counts must sum
/// to the bank's `layerCount`. The bank owns this table until it is released.
typedef struct {
    u8 layerCount; // Number of consecutive `SndBankLayer` layers in this program
    u8 field_1;    // Serialized byte with no individual reader; role unproven
    u8 volume;     // Unsigned Q7 gain; layer volume is multiplied by this / 128
    u8 pan;        // Added to layer pan with 64 removed; 64 leaves layer pan unchanged
} SndBankGroup;
STATIC_ASSERT_SIZEOF(SndBankGroup, 0x4);

/// A sound-bank sample layer and its default voice controls.
///
/// A bank owns `layerCount` consecutive records, partitioned into the runs
/// described by its groups. Layer pointers remain valid until that bank is
/// released or reloaded. A layer index is below its group's `layerCount`.
/// MIDI selects every layer whose inclusive key range
/// contains the played key; scripts select a layer directly and use `keyMin`
/// as the base key for their Q7 pitch offset. Scripts may override the layer's
/// volume, pan and ADSR, and choose reverb independently.
///
/// The serialized sample offset is rebased once after upload, so playback
/// requires the completed load's absolute SPU byte address in `waveAddr`.
typedef struct {
    u8  reverb;   // MIDI reverb send (1 enabled, every other value disabled)
    u8  pan;      // Stereo pan (0 left, 64 centre, 127 right)
    u8  field_2;  // Serialized byte with no individual reader; role unproven
    u8  volume;   // Gain (0 silent, 127 full); MIDI combines group gain / 128
    u8  rootKey;  // MIDI key giving recorded sample pitch when fineTune is zero
    u8  fineTune; // Added playback tuning, in 1/128 semitone steps (0..127)
    u16 priority; // Higher values can steal lower-priority voices; 0 tries range 2 first
    u8  keyMin;   // Inclusive lowest MIDI key; also the script pitch's base key
    u8  keyMax;   // Inclusive highest MIDI key (keyMin <= keyMax <= 127)
    u8  bendDown; // Downward MIDI pitch-bend range, in semitones
    u8  bendUp;   // Upward MIDI pitch-bend range, in semitones
    u16 adsr1;    // Packed SPU attack/decay/sustain-level register
    u16 adsr2;    // Packed SPU sustain/release register
    u32 waveAddr; // Sample byte offset before rebasing, absolute SPU byte address after
} SndBankLayer;
STATIC_ASSERT_SIZEOF(SndBankLayer, 0x14);

/// Mask selecting bits 12..15, the encoded type of a 16-bit sound-bank id.
///
/// The result retains the type in place (0x0000..0xF000) for type-only lookup
/// and allocation/release policy. Shifting it right by 12 gives a slot-map
/// index in 0..15. `SOUND_BANK_ID_FREE` has the same masked type as
/// `SOUND_BANK_TYPE_SEQUENCE`; detecting a free descriptor requires the full id.
enum { SOUND_BANK_TYPE_MASK = 0xF000 };

/// Type-1 bank id with only the type nibble set.
///
/// Bits 12..15 are 1 and the low 12 bits are 0. Type-match lookup compares
/// this with a descriptor id masked by `SOUND_BANK_TYPE_MASK`, so the loaded
/// type-1 bank matches whatever number it carries in the low 12 bits. Retail
/// ids in this band fall between 0x1101 and 0x1521; 0x1000 is the type key,
/// not one of those ids. `SOUND_SCRIPT_REQUEST_TYPE_1` is this key stored in
/// a script request's high half. Slot-map index 1 selects its slot.
enum { SOUND_BANK_TYPE_1 = 0x1000 };
STATIC_ASSERT(SOUND_SCRIPT_REQUEST_TYPE_1 == (SOUND_BANK_TYPE_1 << 16), sound_script_request_type_1);

// Sequence table storage is retained across reloads; the free id belongs to
// that same band.
enum {
    SOUND_BANK_TYPE_SEQUENCE = 0xF000,
    SOUND_BANK_ID_FREE       = 0xFFFF
};

// Sequence table preallocation size in bytes; reused layouts must fit the retained block.
enum { SOUND_BANK_SEQUENCE_TABLE_BYTES = 0x582 };

/// Runtime sound-bank descriptor: program/layer tables and their SPU sample pool.
///
/// The loader partitions `heapBlock` into `groupCount` groups, `layerCount`
/// layers and `groupCount` first-layer indices, in that order. Group layer
/// counts sum to `layerCount`; each index is an element offset into `layers`.
/// Playback requires a completed load: indices have been built and each layer's
/// sample offset has been rebased to an absolute SPU byte address.
///
/// Boot preallocations alias all three table pointers to `heapBlock` until a
/// load partitions it. All three tables must fit the block when it is reused.
/// Reloading invalidates table contents and layer pointers.
/// Releasing a non-sequence bank frees its tables and marks `bankId` free;
/// sequence banks retain their shared table storage across releases. The MIDI
/// sequence or script image is a separate allocation, outside this descriptor.
typedef struct {
    SndBankGroup* groups;          // Program records, indexed below groupCount after loading
    SndBankLayer* layers;          // Consecutive sample layers, partitioned by the programs
    u16           bankId;          // High nibble selects bank type; SOUND_BANK_ID_FREE means free
    u8            field_A;         // No individual access; role and representation unproven
    u8            groupCount;      // Loaded program count and length of groupFirstLayer
    u8            layerCount;      // Loaded layer count, equal to the sum of program layer counts
    byte          unknown_D[0x3];  // No individual access; grouping and role unproven
    u16*          groupFirstLayer; // First layer's element index for each program
    u32           waveBytes;       // Sample-pool byte length, used to place the next bank in SPU RAM
    u32           spuAddr;         // Sample-pool base byte address in SPU RAM
    void*         heapBlock;       // Sound-heap table block; sequence storage is retained across reloads
} SndBank;
STATIC_ASSERT_SIZEOF(SndBank, 0x20);

/// A sound-script entry's tagged allocation policy and shared voice controls.
///
/// This is the 16-byte `oneC` prefix of a variable-length bank entry, addressed
/// by an image-relative byte offset in `SndBankHdr::entryOffsets`. Requests read
/// it before queueing playback; the interpreter skips it to reach the commands.
/// The loaded bank image owns the storage and must remain loaded until all
/// queued requests and playing scripts referring to it have finished.
///
/// Each instance occupies one of eight script slots and can start several SPU
/// voices. A free slot is preferred while the same-sound count is below
/// `maxInstances`. Otherwise, after checking the newest same-sound instance's
/// age against `retriggerTicks`, playback replaces the oldest same-sound
/// instance, or a lower-priority instance, or the oldest equal-priority one.
/// An age limit of -1 forbids replacement, including replacement of other sounds.
/// Same-sound matching ignores request-id bits 8..15; flag 0x10 also groups
/// instances whose complete `flags` words are equal.
///
/// `volumeScale` and `panBias` apply to every voice started by the script.
/// Flags: 0x01 permits requests while the bank type is disabled; 0x02 uses the
/// saved, unducked master volume while ducking is active; 0x10 groups by the
/// full flags word; 0x80 rejects requests while the global mute is active.
/// Other bits have no individual readers but remain part of the grouping key.
typedef struct {
    u32  tag;            // Serialized oneC FourCC; requests do not validate it
    u8   unknown_4;      // Serialized byte with no runtime reader; role unproven
    u8   volumeScale;    // Gain (0 silent, 127 unity), combined with master and layer gain / 127^2
    u8   panBias;        // Added to each voice's pan with 64 subtracted (64 leaves pan unchanged)
    u8   maxInstances;   // Same-sound instance threshold for choosing a free script slot
    s16  retriggerTicks; // Minimum newest-instance age in running audio updates (-1 no replacement)
    byte unknown_A[2];   // Serialized bytes with no runtime reader; grouping and role unproven
    u16  priority;       // Can replace lower priorities, or the oldest equal priority
    u16  flags;          // Request gates, unducked volume and same-sound grouping (see above)
} SndScriptEntryControls;
STATIC_ASSERT_SIZEOF(SndScriptEntryControls, 0x10);

/// Header and entry-offset table of an `hONE` sound-script bank image.
///
/// The eight-byte fixed header is followed by `entryCount` unsigned 16-bit
/// offsets. A request's low byte selects a table slot below `entryCount`;
/// zero means no entry, and multiple slots may share an entry. Nonzero offsets
/// address `SndScriptEntryControls` (`oneC`) blocks relative to the image's start.
/// Scripts and their optional `oneA` / `oneE` chunks occupy the same image.
/// The complete table and every referenced block must fit its loaded byte extent;
/// `sizeof(SndBankHdr)` covers only the fixed header, not that extent.
///
/// A completed load transfers the sound-heap image to its `SndBankSlot`.
/// Entry and chunk pointers remain valid until the image is released or reloaded.
typedef struct {
    u32 magic;           // Serialized hONE FourCC (0x454E4F68); not validated by playback
    u16 bankId;          // Serialized bank id; remapping places it in a request's high 16 bits
    u16 entryCount;      // Number of table slots, including slots with no entry
    u16 entryOffsets[0]; // Image-relative byte offsets to oneC blocks (0 absent)
} SndBankHdr;
STATIC_ASSERT_SIZEOF(SndBankHdr, 0x8);

/// Sound-script bank slot owning an image and referring to its sample tables.
///
/// Sixteen stable records are indexed by slot number, 0..15. Bank types select
/// slots through a map; type 4 uses successive slots 4..6. A completed script
/// load transfers its sound-heap `hONE` image here and references a separately
/// managed `SndBank`. MIDI sequence images are held outside these records.
/// Boot can reserve an image buffer before its contents have been loaded.
/// Playback requires a completed image and initialized program/layer tables.
///
/// Lookup compares `bank->bankId`, exactly or by its high-nibble type.
/// `bankId` and `spuAddr` are snapshots with no playback readers. Image release
/// sets `image` to NULL and `bankId` to -1, retaining `bank` and `spuAddr`;
/// it does not release the descriptor's tables or its SPU samples.
/// Deferred events, scripts and chunk pointers require the image and tables
/// to remain loaded until their last use. Reloading replaces their contents.
typedef struct {
    SndBankHdr* image;   // Owned sound-heap script image or boot reservation (NULL after release)
    SndBank*    bank;    // Borrowed program/layer descriptor; retained after image release
    s32         bankId;  // Snapshot of the 16-bit bank id (boot id before loading, -1 after release)
    u32         spuAddr; // SPU sample-pool origin in bytes, before any upload-block offset
} SndBankSlot;
STATIC_ASSERT_SIZEOF(SndBankSlot, 0x10);

/// Sequence selection, gain and fade timing for deferred MIDI commands.
///
/// `sequenceId` is matched against the loaded sequence's id when the event is
/// processed; it is not a slot index. Zero selects all sequences for stop,
/// mute, unmute and volume commands, but start matches it exactly. Enqueuers
/// reject 255, the unloaded-sequence marker.
///
/// Start and stop read `fadeTicks`; mute and unmute use fixed ramps and read
/// only `sequenceId`. Volume commands read `volumeScale`, which multiplies the
/// sequence's mix-table level; master volume and the fade are applied separately.
/// Volume commands require a loaded sequence with an id in 0..99, including
/// when the selector is zero. Sequence 0x5A uses a fixed gain instead of this
/// requested gain. Enqueuers map gain bytes with bit 7 set to 127.
/// Unused fields retain the previous event's bytes and must not be read.
///
/// Fade timing is in audio updates, including the extra PAL timer updates,
/// rather than rendered frames. The unsigned 16-bit request sets the ramp step
/// to 65535 / fadeTicks; rounding can extend the fade. Stop requests clear the
/// low two bits before queueing. Zero bypasses interpolation.
typedef struct {
    u8  sequenceId;  // Loaded sequence id (0 all except for start; 255 rejected)
    u8  volumeScale; // Requested sequence gain (0 silent, 127 full), independent of master volume
    u16 fadeTicks;   // Requested audio-update count (0 no interpolation)
} SndEvtMidiArgs;
STATIC_ASSERT_SIZEOF(SndEvtMidiArgs, 0x4);

/// Deferred sound script playback, stop, mute and mix control arguments.
///
/// Each script instance can own several SPU voices. Start reads `soundId`,
/// `panOffset`, `level.attenuation`, `bankSlot` and `entryControls`; stop reads
/// `soundId` and `stopControl`; mute/unmute read only `soundId`. Pan/attenuation
/// and volume updates change the first active instance whose id matches exactly.
/// Unused fields retain the previous event's bytes and must not be read.
///
/// The id's high half selects the bank, its low byte selects the script entry,
/// and bits 8..15 distinguish instances. Stop also accepts 0xFF in bits 8..15
/// to match all instances of an entry. A stop selector containing only the high
/// nibble selects that bank type; 0x80000000 stops every type except 6.
/// Mute/unmute match either the exact id or a high-nibble bank-type selector.
///
/// `panOffset` adds three SPU pan steps per unit to each voice's base pan.
/// Attenuation scales each voice's volume-table index by (127 - magnitude) / 127
/// for magnitudes 0..127; the sign can carry source depth. Volume updates instead
/// request a scale 0..127; enqueuers map bytes with bit 7 set to 127. The signed
/// value -128 is retained: start scales the index by 1/127, while a
/// pan/attenuation update targets the unattenuated level.
///
/// Stop 0 requests SPU release rate 5; 1 keeps the existing release settings.
/// For a running entry selected by id, values 2..65535 fade before release.
/// The ramp advances by 65535 / stopControl per running audio update, including
/// extra PAL timer updates; rounding can extend the fade. Bank-type stops ignore
/// fade durations and request release rate 5 unless the control is 1.
///
/// Start borrows a stable bank slot and its image's oneC controls. The image and
/// sample tables must remain loaded through queued playback and script execution.
typedef struct {
    s8 panOffset;                          // Signed mix-pan offset (0 unchanged, 3 SPU pan steps per unit)
    union {
        s8 attenuation;                    // Signed source-depth attenuation (0 full; magnitude 127 silent)
        u8 volumeScale;                    // Requested mix level scale (0 silent, 127 full)
    } level;
    u16                     stopControl;   // 0 override release, 1 keep release, 2..65535 fade in audio updates
    s32                     soundId;       // Bank-remapped script request id or command-specific selector
    SndBankSlot*            bankSlot;      // Borrowed bank slot resolved when start is queued
    SndScriptEntryControls* entryControls; // Borrowed oneC prefix of the script entry to start
} SndEvtScriptArgs;
STATIC_ASSERT_SIZEOF(SndEvtScriptArgs, 0x10);

/// Deferred MIDI-sequence or sound-script arguments selected by `SndEvt::command`.
///
/// Commands 1..5 read `midi`: start, stop, mute, unmute and volume.
/// Commands 6..11 read `script`: start, stop, mute, unmute, pan/attenuation
/// and volume. Commands 0 and 12..15 do not read either arm.
///
/// The event slot owns this storage until processing releases it for reuse.
/// Allocation and release do not clear the arguments; producers must initialize
/// every field their command reads, and consumers must not read the remaining
/// bytes or the other arm. Script-start pointers borrow bank-slot and script-image
/// data that must remain valid through deferred processing and script playback.
typedef union {
    SndEvtMidiArgs   midi;   // Sequence selection, gain and fade timing (commands 1..5)
    SndEvtScriptArgs script; // Script-instance selection and playback/mix controls (commands 6..11)
} SndEvtArgs;
STATIC_ASSERT_SIZEOF(SndEvtArgs, 0x10);

/// Command codes carried by `SndEvt::command`, in audio dispatch order.
///
/// MIDI commands read `args.midi`; script commands 6..11 read `args.script`.
/// `SOUND_EVENT_NO_OP` and `SOUND_EVENT_RESERVED_NO_OP` perform no audio work
/// and read no arguments. Reserving a slot stores `SOUND_EVENT_NO_OP` until
/// the producer replaces `command`. Commands 13..15 take no arguments. Duck
/// acquire/release count requests to lower the script master level toward 48
/// and restore its saved level when the last request ends; scripts may opt out
/// of that attenuation. Key off releases script voices in bank types 1 and 5
/// during stage changes.
enum {
    SOUND_EVENT_NO_OP           = 0,
    SOUND_EVENT_MIDI_START      = 1,
    SOUND_EVENT_MIDI_STOP       = 2,
    SOUND_EVENT_MIDI_MUTE       = 3,
    SOUND_EVENT_MIDI_UNMUTE     = 4,
    SOUND_EVENT_MIDI_SET_VOLUME = 5,
    /// Requests a sound-script instance for deferred audio playback.
    ///
    /// Reads `args.script.soundId`, `panOffset`, `level.attenuation`,
    /// `bankSlot` and `entryControls`; `stopControl` is unused.
    /// Dispatch chooses one of eight script slots using the entry's priority,
    /// instance limit and retrigger policy. A rejected request is silently
    /// dropped; an accepted one starts the interpreter, whose commands allocate
    /// SPU voices. Queueing alone does not guarantee playback.
    /// The borrowed bank slot must remain stable, and its script image and
    /// sample tables must remain loaded through queued dispatch and execution.
    SOUND_EVENT_SCRIPT_START               = 6,
    SOUND_EVENT_SCRIPT_STOP                = 7,
    SOUND_EVENT_SCRIPT_MUTE                = 8,
    SOUND_EVENT_SCRIPT_UNMUTE              = 9,
    SOUND_EVENT_SCRIPT_SET_PAN_ATTENUATION = 10,
    SOUND_EVENT_SCRIPT_SET_VOLUME          = 11,
    SOUND_EVENT_RESERVED_NO_OP             = 12,
    SOUND_EVENT_SCRIPT_DUCK_ACQUIRE        = 13,
    SOUND_EVENT_SCRIPT_DUCK_RELEASE        = 14,
    SOUND_EVENT_SCRIPT_KEY_OFF             = 15
};

/// Pool slot for one deferred audio command and its FIFO queue links.
///
/// Resident sound producers reserve a slot with `sndEvtAlloc`, set `command`
/// and every argument that command reads, then pass it once to `sndEvtEnqueue`.
/// The audio update processes queued slots in order and releases each afterward.
/// Allocation starts with `SOUND_EVENT_NO_OP`; allocation and release leave the
/// argument storage intact. Borrowed script-start data must remain loaded through
/// processing and script playback. Reset or an invalid command discards the
/// whole pool, including reserved slots that have not been queued yet.
///
/// `prev` records the predecessor when appended; draining does not repair the
/// new head's backward link. Released slots have both links cleared.
typedef struct SndEvt {
    s16            allocated; // Slot reservation (0 free, 1 reserved or queued)
    s16            command;   // SOUND_EVENT_ command (0..15); selects the argument interpretation
    SndEvtArgs     args;      // Command payload; only the selected command's inputs are initialized
    struct SndEvt* prev;      // Predecessor saved at enqueue; may already be released while draining
    struct SndEvt* next;      // Next queued slot, or NULL at the tail
} SndEvt;
STATIC_ASSERT_SIZEOF(SndEvt, 0x1C);

/// Five-word sound-bank sector header, retained throughout an incremental load.
/// The bank id's high nibble selects the bank type; groups and notes determine
/// the descriptor allocation, while the final fields size the streamed data.
typedef struct _SndBankPayload {
    /* 0x00 */ u32 tag;
    /* 0x04 */ u16 bankId;
    /* 0x06 */ u8  variant;
    /* 0x07 */ u8  groupCount;
    /* 0x08 */ u8  noteCount;
    /* 0x09 */ u8  unknown_9;
    /* 0x0A */ u16 waveBlockOffset; // SPU upload offset, in 64-byte blocks
    /* 0x0C */ u8  transferSectors;
    /* 0x0D */ u8  unknown_D;
    /* 0x0E */ u16 imageBytes;
    /* 0x10 */ s32 waveBytes;
} SndBankPayload;
STATIC_ASSERT_SIZEOF(SndBankPayload, 0x14);

typedef union {
    SndBankPayload header;
    u32            words[5];
} SndLoadPayload;
STATIC_ASSERT_SIZEOF(SndLoadPayload, 0x14);

/// State block at SndLoad_State.
/// imageBuffer and bank are cleared by Snd_InitFromStage; SndLoad_Init sets field_10.
/// The CD-ready callback resets the payload transfer offset and sector count.
/// SndLoad_ProcessSector copies the complete five-word header into payload.
/// Named BSS symbols D_80082120+ begin immediately after this 0x30-byte block.
typedef struct _SndLoadState {
    /* 0x00 */ u8             field_0;
    /* 0x01 */ u8             field_1;
    /* 0x02 */ u8             field_2;
    /* 0x03 */ u8             field_3;
    /* 0x04 */ void*          sectorBuffer;
    /* 0x08 */ u8*            writeCursor;
    /* 0x0C */ s32            field_C;
    /* 0x10 */ s32            field_10;
    /* 0x14 */ void*          imageBuffer;
    /* 0x18 */ SndBank*       bank;
    /* 0x1C */ SndLoadPayload payload;
} SndLoadState;
STATIC_ASSERT_SIZEOF(SndLoadState, 0x30);

/// Out-parameter for `Spu_GetVoiceRef` (voice slot lookup/alloc).
/// field_0 = voice index; field_4 = SpuVoiceAttr*.
typedef struct _SpuVoiceRef {
    /* 0x0 */ s8            field_0; // voiceIdx
    /* 0x1 */ s8            field_1;
    /* 0x2 */ s8            field_2;
    /* 0x3 */ s8            field_3;
    /* 0x4 */ SpuVoiceAttr* field_4; // attr
} SpuVoiceRef;
STATIC_ASSERT_SIZEOF(SpuVoiceRef, 0x8);

/// Status word of an AsyncCbEntry. The queue manipulates it as a whole word;
/// the poll callback reads and advances its own fields.
typedef union {
    s32 word;
    struct {
        u32 : 1;
        /// Set when the entry is queued, cleared by the poll callback on its
        /// first call.
        u32 firstPoll : 1;
        u32           : 2;
        /// Step of the poll callback's own state machine, zeroed on queueing.
        u32 pollState : 8;
    } bits;
} AsyncCbFlags;

/// Callback-queue slot used by AsyncCb_Queue.entries (stride 0x14).
/// field_0 flags: bit0 active, bit1 arm, bit2 pending, bit3 result.
typedef struct _AsyncCbEntry {
    /* 0x00 */ AsyncCbFlags field_0;                            // flags
    /* 0x04 */ s32          field_4;                            // data
    /* 0x08 */ s32          (*field_8)(struct _AsyncCbEntry*);  // pollFn
    /* 0x0C */ void         (*field_C)(struct _AsyncCbEntry*);  // doneFn
    /* 0x10 */ s32          (*field_10)(struct _AsyncCbEntry*); // errorFn
} AsyncCbEntry;
STATIC_ASSERT_SIZEOF(AsyncCbEntry, 0x14);

#endif // MAIN_PRIVATE_SOUND_TYPES_H
