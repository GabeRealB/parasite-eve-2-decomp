#ifndef MAIN_PRIVATE_SOUND_TYPES_H
#define MAIN_PRIVATE_SOUND_TYPES_H

#include <psyq/sys/types.h>
#include <psyq/libspu.h>

#include "common.h"

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

// Bank ids encode their storage class in the high nibble. Sequence table
// storage is retained across reloads; the free id belongs to that same band.
enum {
    SOUND_BANK_TYPE_MASK     = 0xF000,
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

/// Arguments of the voice commands, which address a sound-bank entry and the
/// voices playing from it: allocate a voice, ramp its pan or volume, and stop
/// the voices a sound id matches.
///
/// Every command reads `id` and leaves the fields it does not read holding
/// whatever the slot's previous occupant wrote: starting a voice reads `pan`,
/// `level.attenuation`, `bank` and `params`, the pan ramp reads `pan` and
/// `level.attenuation`, the volume ramp reads `level.loudness`, and the stop
/// reads `stopFrames`.
///
/// `level` is one byte the commands read on two opposite scales. A command that
/// places a source states how far it is from the listener, so 0 is the
/// listener's own distance and the full level, and a source behind the listener
/// is the negative of its distance. The volume ramp instead states the level to
/// move to, on the scale where 0x7F is full. `stopFrames` is not a length alone
/// either: 0 stops the matched voices at once, 1 does the same and raises a flag
/// on the slot, and a larger value is the fade length in frames.
typedef struct {
    s8 pan;                             // Stereo offset from the voice's own pan (0 centre)
    union {
        s8 attenuation;                 // How far the source is from the listener, subtracted from its level
        u8 loudness;                    // Level the volume ramp moves to (0x7F full, 0 silent)
    } level;
    u16                     stopFrames; // How a matching stop acts on the voices
    s32                     id;         // Bank-remapped id of the sound the event acts on
    SndBankSlot*            bank;       // Bank the id was resolved in, held for the deferred start
    SndScriptEntryControls* params;     // Bank entry the voice is started from
} SndEvtVoiceArgs;
STATIC_ASSERT_SIZEOF(SndEvtVoiceArgs, 0x10);

/// Arguments of a `SndEvt`, one arm per family of handlers.
///
/// An event owns a single argument slot, and the two arms are the layouts its
/// commands read it under: the sequence commands, which address a `MidiSong`,
/// read `midi`, and the voice commands, which address a bank entry and the
/// voices started from it, read `voice`. The slot is as large as the larger of
/// the two, and a command fills in only the fields its own handler reads, so
/// the rest of the slot still holds what its previous occupant left there.
typedef union {
    SndEvtMidiArgs  midi;
    SndEvtVoiceArgs voice;
} SndEvtArgs;
STATIC_ASSERT_SIZEOF(SndEvtArgs, 0x10);

/// Deferred sound event: one queued audio command, in a slot of `_gSndEvtPool`.
///
/// An event is filled in and then passed to `sndEvtEnqueue`, which appends it to
/// the pending queue; `SndEvt_Process` passes the oldest event to the handler
/// `handlerIdx` selects and returns the slot to the pool. A freed slot is only
/// marked, never cleared, so filling one in writes every argument its handler
/// reads.
typedef struct SndEvt {
    s16            allocated;  // 0 free, 1 in use
    s16            handlerIdx; // Which command the event carries; indexes SndEvt_Handlers
    SndEvtArgs     args;       // Arguments, read according to the command
    struct SndEvt* prev;       // Previous event in the pending queue
    struct SndEvt* next;       // Next event in the pending queue
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
