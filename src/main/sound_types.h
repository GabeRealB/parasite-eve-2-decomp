#ifndef MAIN_PRIVATE_SOUND_TYPES_H
#define MAIN_PRIVATE_SOUND_TYPES_H

#include <psyq/sys/types.h>
#include <psyq/libspu.h>

#include "common.h"

struct SndBank;

/// Per-frame audio callback; return -1 to remove the registration.
typedef s32 (*AudioTickPoll)(s32* arg);

typedef void (*AudioTickOnRemove)(void);

/// Per-voice SPU runtime (Spu_VoiceState). 24 voices.
typedef void (*SpuVoiceCallback)(void* context);

typedef struct SndBank SndBank;

/// The voice parameters a sound-bank entry starts a sound with, carried as the
/// `oneC` command that opens the entry's script program.
///
/// A bank publishes its entries through `SndBankHdr::entryOffsets`, and an entry
/// is a small script whose first command is this block, followed by the `oneV`
/// commands that start the voices. The allocator also reads the block when the
/// sound is requested, so it settles which script slot the sound gets: a request
/// plays while fewer than `maxVoices` copies of the sound are running and a slot
/// is free, and otherwise either restarts the newest copy or takes over the
/// playing sound with the lowest `priority` — and is dropped instead, when the
/// newest copy is younger than `retriggerFrames` (a `-1` refuses every takeover,
/// so the sound plays only while a slot is free).
///
/// `flags` carries the sound's playback switches: (bit 0 plays while its sound
/// type is disabled, bit 1 follows the override level rather than the master
/// volume, bit 4 groups the entries that share its value as copies of one sound,
/// bit 7 is dropped while the sound is muted).
typedef struct {
    s32 magic;           // "oneC" (0x43656E6F) — the command this block carries
    u8  unknown_4;
    u8  volume;          // Volume scale applied over the note's (0-127)
    u8  pan;             // Pan (0x40 = centre)
    u8  maxVoices;       // Copies of this sound allowed to play at once
    s16 retriggerFrames; // Age a playing copy must reach before another request takes a slot
    u16 unknown_A;
    u16 priority;        // Allocation priority: the lowest playing sound gives up its slot first
    u16 flags;           // Playback switches (see above)
} SndVoiceParams;
STATIC_ASSERT_SIZEOF(SndVoiceParams, 0x10);

/// Header of the sound-bank image held by a `SndBankSlot`.
///
/// The image is one allocation: this header, then the table of entry offsets
/// declared below, then each entry's `SndVoiceParams` block and the `oneA` /
/// `oneE` chunks, all addressed by byte offset from the header. An entry is
/// selected by the low byte of a sound request id and its offset is where that
/// entry's block begins.
typedef struct {
    u8  unknown_0[4];
    u16 bankId;          // Supplies the high half of a 0x1xxx request id
    u16 entryCount;      // Entries in the offset table below
    u16 entryOffsets[0]; // Byte offsets to each entry's `SndVoiceParams`, relative to this header
} SndBankHdr;
STATIC_ASSERT_SIZEOF(SndBankHdr, 0x8);

/// One record of `_gSndBankSlots`, holding the sound bank a slot has loaded.
///
/// The bank is held twice over: the image, which a script reads its entry
/// offsets and `oneA` chunks from, and the descriptor, which holds the notes a
/// voice plays. Lookup matches the descriptor's own id, exactly or by its
/// 0xF000 group; releasing the record hands the image back to the sound heap
/// and sets `bankId` to -1.
typedef struct {
    SndBankHdr* image;   // Bank image in the sound heap, whose head holds the entry offsets
    SndBank*    bank;    // Descriptor of the loaded bank, in `Snd_Banks`
    s32         bankId;  // Id of the bank held here (-1 once the record is free)
    u32         spuAddr; // SPU RAM address the bank's wave data was transferred to
} SndBankSlot;
STATIC_ASSERT_SIZEOF(SndBankSlot, 0x10);

/// Arguments of the sequence commands, which address a `MidiSong` rather than a
/// sound-bank voice: initialize a sequence, start and stop its fades, and set
/// its volume scale.
///
/// Every command reads `song`. The two that take a fade length read
/// `fadeFrames` — initializing a sequence fades it in, and the fade-out command
/// fades it out — while the one that re-scales a sequence reads `volumeScale`,
/// so a command leaves any field it does not read holding whatever the slot's
/// previous occupant wrote. The commands acting on an already-loaded sequence
/// treat a `song` of 0 as every loaded one; initializing a sequence matches the
/// id exactly instead.
typedef struct {
    u8  song;        // Which loaded sequence the command acts on
    u8  volumeScale; // Scale applied over the master volume (0 silent, 0x7F full)
    u16 fadeFrames;  // Length of the fade the command starts, in frames (0 no fade)
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
    s8 pan;                     // Stereo offset from the voice's own pan (0 centre)
    union {
        s8 attenuation;         // How far the source is from the listener, subtracted from its level
        u8 loudness;            // Level the volume ramp moves to (0x7F full, 0 silent)
    } level;
    u16             stopFrames; // How a matching stop acts on the voices
    s32             id;         // Bank-remapped id of the sound the event acts on
    SndBankSlot*    bank;       // Bank the id was resolved in, held for the deferred start
    SndVoiceParams* params;     // Bank entry the voice is started from
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

/// A sound-bank program's layer count and shared volume and pan controls.
///
/// Records are indexed by program number, below the bank's `groupCount`.
/// Each describes a consecutive run of `SndBankLayer` layers; their counts must sum
/// to the bank's `noteCount`. The bank owns this table until it is released.
typedef struct {
    u8 layerCount; // Number of consecutive `SndBankLayer` layers in this program
    u8 field_1;    // Serialized byte with no individual reader; role unproven
    u8 volume;     // Unsigned Q7 gain; layer volume is multiplied by this / 128
    u8 pan;        // Added to layer pan with 64 removed; 64 leaves layer pan unchanged
} SndBankGroup;
STATIC_ASSERT_SIZEOF(SndBankGroup, 0x4);

/// A sound-bank sample layer and its default voice controls.
///
/// A bank owns `noteCount` consecutive records, partitioned into the runs
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

/// One entry of `Snd_Banks`: the tables a bank image's programs play their notes
/// from, plus what the loader recorded about the image they came out of.
///
/// A bank is filled either from an image the loader reads out of a CD sector and
/// streams into SPU RAM, or from `Snd_BankInitTable` for the banks that are
/// resident from boot; a slot whose `bankId` is 0xFFFF is free. The group, note
/// and index tables live in one `SndHeap` block, laid out in that order as the
/// image is loaded, so its own counts decide where each table starts. A note's
/// waveform address is relative to the image's SPU address and the loader
/// rebases it once the image has been placed.
struct SndBank {
    SndBankGroup* groups;     // Group table, one group per program
    SndBankLayer* notes;      // Note layers of every group, one group after another
    u16           bankId;     // Bank id; 0xFFFF marks a free slot
    u8            field_A;    // Role unproven
    u8            groupCount; // Groups in `groups`, and the length of the index table
    u8            noteCount;  // Notes in `notes`, the groups' counts summed
    byte          unknown_D[0x3];
    u16*          groupIndex; // First note of each group, indexed by program
    u32           imageSize;  // Byte length of the image the bank was loaded from
    u32           spuAddr;    // SPU RAM address the image's waveform data was loaded to
    void*         heapBlock;  // `SndHeap` block the three tables are carved from
};
STATIC_ASSERT_SIZEOF(SndBank, 0x20);

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
