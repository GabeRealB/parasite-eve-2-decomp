#ifndef SOUND_H
#define SOUND_H

#include "common.h"

#include <psyq/libspu.h>

#include "main/mem.h"

#define SNDHEAP_SIZE        0x3D00
#define SNDHEAP_START_MAGIC 0xB25A
#define SNDHEAP_MAGIC       0xA52B

// Types — SPU / MIDI / SndEvt / SndScript / bank / AsyncCb

typedef struct _SpuReverbConfig {
    u32           enableVoices;
    u32           disableVoices;
    u32           reverbMode;
    u32           isDirty;
    SpuReverbAttr attr;
} SpuReverbConfig;
STATIC_ASSERT_SIZEOF(SpuReverbConfig, 0x24);

/// Per-frame audio callback list node (AudioTick_List sentinel + chain).
typedef struct _AudioTickNode {
    /* 0x00 */ s32 poll;     // s32 (*)(s32) — return -1 to remove
    /* 0x04 */ s32 onRemove; // void (*)(void)
    /* 0x08 */ s16 id;
    /* 0x0C */ s32 arg;
    /* 0x10 */ s32 prev;
    /* 0x14 */ s32 next;
} AudioTickNode;
STATIC_ASSERT_SIZEOF(AudioTickNode, 0x18);

/// Per-voice SPU runtime (Spu_VoiceState). 24 voices.
typedef struct _SpuVoiceState {
    /* 0x000 */ u32 reverbVoiceStatus;
    /* 0x004 */ u32 field_4[24];   // age / score for voice steal
    /* 0x064 */ u8  field_64[24];  // state (0/3 ≈ free-ish)
    /* 0x07C */ u8  field_7c[24];  // key-on staging (5 on note-on)
    /* 0x094 */ s8  field_94[24];  // occupied (0 free, 1 busy)
    /* 0x0AC */ u32 field_ac[24];  // alloc priority
    /* 0x10C */ u32 field_10c[24]; // cleared on release
    /* 0x16C */ u32 field_16c[24]; // cleared on release
    /* 0x1CC */ u32 field_1cc;
    /* 0x1D0 */ u32 field_1d0;     // key-on related mask
} SpuVoiceState;
STATIC_ASSERT_SIZEOF(SpuVoiceState, 0x1D4);

typedef struct _SpuLVoiceTable {
    /* 0x000 */ s16           count;         // active attr count
    /* 0x002 */ SpuLVoiceAttr attrs[24];
    /* 0x664 */ u8            field_664[24]; // per-voice flags
} SpuLVoiceTable;
STATIC_ASSERT_SIZEOF(SpuLVoiceTable, 0x67C);

/// 0x1C-byte slot allocated from SndEvt_Pool (see SndEvt_Alloc / SndEvt_Enqueue).
/// Overlay of `SndEvt` starting at offset 0x4 (`field_4` / `field_8`).
/// Used when the compiler keeps `arg + 4` in a callee-saved register.
/// Overlay of SndEvt starting at field_4 (handler payload).
typedef struct _SndEvtFrom4 {
    /* 0x0 */ u8  field_0; // SndEvt.field_4
    /* 0x1 */ u8  field_1; // SndEvt.field_5
    /* 0x2 */ u16 field_2; // SndEvt.field_6
    /* 0x4 */ s32 field_4; // payload
    /* 0x8 */ s32 field_8; // payload
    /* 0xC */ s32 field_C; // often SndVoiceParams*
} SndEvtFrom4;
STATIC_ASSERT_SIZEOF(SndEvtFrom4, 0x10);

/// Deferred sound/MIDI event message (SndEvt_Pool, 0x40 slots).
typedef struct _SndEvt {
    /* 0x00 */ s16             allocated; // 0 free, 1 in use
    /* 0x02 */ s16             handlerIdx;
    /* 0x04 */ u8              field_4;   // handler payload
    /* 0x05 */ u8              field_5;
    /* 0x06 */ u16             field_6;
    /* 0x08 */ s32             field_8;
    /* 0x0C */ s32             field_C;
    /* 0x10 */ s32             field_10;
    /* 0x14 */ struct _SndEvt* prev;
    /* 0x18 */ struct _SndEvt* next;
} SndEvt;
STATIC_ASSERT_SIZEOF(SndEvt, 0x1C);

/// 8-byte slot at MidiOpcodeCtx::field_484 (16 entries, indexed by opcode low nibble).
/// Seeded as the word 0x407F4000 by Midi_InitChannelTable (field_0..field_3 little-endian);
/// field_0 is tested as a flag byte (lbu); field_4 is a byte written by handlers.
typedef struct _MidiOpcodeSlot {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ u8  field_2;
    /* 0x3 */ u8  field_3;
    /* 0x4 */ u8  field_4;
    /* 0x5 */ u8  field_5;
    /* 0x6 */ s16 field_6;
} MidiOpcodeSlot;
STATIC_ASSERT_SIZEOF(MidiOpcodeSlot, 0x8);

/// Large context object used by 410B0.c opcode handlers (e.g. Midi_SetProgram).
/// Only fields used so far are named; size is incomplete.
typedef struct _MidiOpcodeCtx {
    /* 0x000 */ byte           unknown_0[0x484];
    /* 0x484 */ MidiOpcodeSlot field_484[16];
} MidiOpcodeCtx;

/// 4-byte entry at Spu_VoiceRanges (see Spu_SetVoiceRange).
typedef struct _SpuVoiceRange {
    /* 0x0 */ s16 first;
    /* 0x2 */ s16 count;
} SpuVoiceRange;
STATIC_ASSERT_SIZEOF(SpuVoiceRange, 0x4);

/// Header for the bank table blob pointed to by SndBankSlot.field_0.
/// field_4 is the bank ID (high halfword remapped by SndBank_RemapId when the
/// request high nibble is 0x1); field_6 is the entry count used by SndEvt_EnqueueType6.
/// A u16 offset table follows at 0x8 (indexed via SndBankHdrOff).
typedef struct _SndBankHdr {
    /* 0x0 */ u8  unknown_0[4];
    /* 0x4 */ u16 field_4;
    /* 0x6 */ u16 field_6;
} SndBankHdr;
STATIC_ASSERT_SIZEOF(SndBankHdr, 0x8);

/// Overlay for reading the u16 offset table that follows SndBankHdr at +0x8.
/// Formed as (SndBankHdrOff*)((index * 2) + (s32)header) so lhu 8(base)
/// picks offsets[index] (SndEvt_EnqueueType6).
typedef struct _SndBankHdrOff {
    /* 0x0 */ u8  pad[8];
    /* 0x8 */ u16 field_8;
} SndBankHdrOff;
STATIC_ASSERT_SIZEOF(SndBankHdrOff, 0xA);

/// 16-byte slot in SndBank_Slots[16] (BSS size 0x100). Indexed by SndBankSlot_Get
/// and related helpers in 43FFC.c / 410B0.c.
typedef struct _SndBankSlot {
    /* 0x0 */ SndBankHdr* field_0;
    /* 0x4 */ void*       field_4;
    /* 0x8 */ s32         field_8;
    /* 0xC */ void*       field_C;
} SndBankSlot;
STATIC_ASSERT_SIZEOF(SndBankSlot, 0x10);

/// Owner of a doubly-linked SndVoice voice list (head at field_40).
/// Insert: SndVoice_Attach; unlink: SndVoice_Detach; walk: SndScript_TickVoices.
/// field_44 is a pointer to the raw oneE/script buffer base (SndVoice_SetupEnvelope).
typedef struct _SndVoice SndVoice;

typedef struct _SndVoiceOwner {
    /* 0x00 */ u8        unknown_0[0x40];
    /* 0x40 */ SndVoice* field_40;
    /* 0x44 */ u8**      field_44;
} SndVoiceOwner;
STATIC_ASSERT_SIZEOF(SndVoiceOwner, 0x48);

/// "oneE" (0x45656E6F) pitch-envelope chunk pointed at by SndVoiceFx.field_20.
/// Consumed by the state machine in SndVoice_TickEnvelope.
typedef struct _SndOneE {
    /* 0x00 */ s32 magic;
    /* 0x04 */ s16 field_4;
    /* 0x06 */ s16 field_6;
    /* 0x08 */ s16 field_8;
    /* 0x0A */ u16 field_A;
    /* 0x0C */ s16 field_C;
    /* 0x0E */ u16 field_E;
    /* 0x10 */ s16 field_10;
    /* 0x12 */ s16 field_12;
    /* 0x14 */ s16 field_14;
    /* 0x16 */ s16 field_16;
} SndOneE;
STATIC_ASSERT_SIZEOF(SndOneE, 0x18);

/// FX/envelope sub-block embedded at SndVoice + 0x10 (SndVoice_SetupEnvelope / SndVoice_TickEnvelope).
/// field_0 is an active flag; field_1 is the state-machine index; field_2 is a
/// secondary gate; field_20 points at the current "oneE" (0x45656E6F) chunk.
typedef struct _SndVoiceFx {
    /* 0x00 */ s8       field_0;
    /* 0x01 */ s8       field_1;
    /* 0x02 */ s8       field_2;
    /* 0x03 */ u8       pad_3;
    /* 0x04 */ s32      field_4;
    /* 0x08 */ s16      field_8;
    /* 0x0A */ s16      field_A;
    /* 0x0C */ u16      field_C;
    /* 0x0E */ s16      field_E;
    /* 0x10 */ s32      field_10;
    /* 0x14 */ s32      field_14;
    /* 0x18 */ s32      field_18;
    /* 0x1C */ s32      field_1C;
    /* 0x20 */ SndOneE* field_20;
} SndVoiceFx;
STATIC_ASSERT_SIZEOF(SndVoiceFx, 0x24);

/// Voice/FX object carved from SndBank_Slots with stride 0x40 (SndVoice_Alloc).
/// field_0 is the SPU voice index; field_4 is a countdown/timer (SndVoice_Tick).
/// field_10/field_12 gate FX processing (aliases of fx.field_0 / fx.field_2).
/// field_34/field_38/field_3C are parent/prev/next list links (SndVoice_Detach free).
struct _SndVoice {
    /* 0x00 */ s8             field_0;
    /* 0x01 */ u8             field_1;
    /* 0x02 */ s8             field_2;
    /* 0x03 */ u8             field_3;
    /* 0x04 */ s32            field_4;
    /* 0x08 */ s16            field_8;
    /* 0x0A */ u8             field_A;
    /* 0x0B */ u8             pad_0B;
    /* 0x0C */ void*          field_C; // current oneV/script command (SndScript_Exec)
    /* 0x10 */ s8             field_10;
    /* 0x11 */ s8             field_11;
    /* 0x12 */ s8             field_12;
    /* 0x13 */ u8             unknown_13;
    /* 0x14 */ s32            field_14;
    /* 0x18 */ s16            field_18;
    /* 0x1A */ s16            field_1A;
    /* 0x1C */ s16            field_1C;
    /* 0x1E */ u8             unknown_1E[0x6];
    /* 0x24 */ s32            field_24;
    /* 0x28 */ s32            field_28;
    /* 0x2C */ s32            field_2C;
    /* 0x30 */ s32*           field_30;
    /* 0x34 */ SndVoiceOwner* field_34;
    /* 0x38 */ SndVoice*      field_38;
    /* 0x3C */ SndVoice*      field_3C;
};
STATIC_ASSERT_SIZEOF(SndVoice, 0x40);

/// Overlay of SndLoadState at +0x1C (sector payload header for sound-bank setup).
/// Passed to Snd_AllocBank; filled from a CD sector by SndLoad_ProcessSector.
/// field_4 high nibble indexes D_800680AC / selects bank type; field_7 is the
/// SndBankGroup count and field_8 is the SndNote entry count used to size
/// the SndBank heap block (groups*4 + entries*0x14 + groups*2).
typedef struct _SndBankPayload {
    /* 0x0 */ u8  pad_0[4];
    /* 0x4 */ u16 field_4;
    /* 0x6 */ u8  field_6;
    /* 0x7 */ u8  field_7;
    /* 0x8 */ u8  field_8;
} SndBankPayload;

/// State block at SndLoad_State.
/// field_14/field_18 cleared by Snd_InitFromStage; field_10 sized by SndLoad_Init.
/// field_26/field_28 set by the CD ready path in CdAudio_FeedSector.
/// field_1C..field_2C are filled as five words from a sector by SndLoad_ProcessSector
/// (overlay of SndBankPayload at +0x1C: field_20/22/23/24 == payload field_4/6/7/8).
/// Named BSS symbols D_80082120+ begin immediately after this 0x30-byte block.
typedef struct _SndLoadState {
    /* 0x00 */ u8    field_0;
    /* 0x01 */ u8    field_1;
    /* 0x02 */ u8    field_2;
    /* 0x03 */ u8    field_3;
    /* 0x04 */ void* field_4;
    /* 0x08 */ s32   field_8;
    /* 0x0C */ s32   field_C;
    /* 0x10 */ s32   field_10;
    /* 0x14 */ s32   field_14;
    /* 0x18 */ s32   field_18;
    /* 0x1C */ u32   field_1C;
    /* 0x20 */ u16   field_20;
    /* 0x22 */ u8    field_22;
    /* 0x23 */ u8    field_23;
    /* 0x24 */ u8    field_24;
    /* 0x25 */ u8    field_25;
    /* 0x26 */ u16   field_26;
    /* 0x28 */ u8    field_28;
    /* 0x29 */ u8    field_29;
    /* 0x2A */ u16   field_2A;
    /* 0x2C */ s32   field_2C;
} SndLoadState;
STATIC_ASSERT_SIZEOF(SndLoadState, 0x30);

/// Track/channel entry inside MidiSong (stride 0x3C). field_5 is a per-entry flag
/// written by Midi_ResetTrackFlags; absolute offset of first entry's field_5 is 0x51.
/// field_0 / field_1 / field_4 are NRPN/RPN state used by the MIDI CC handler
/// (func_80052488). field_6 / field_7 and field_8[] form a loop stack for the
/// 0xF5/0xF6 meta opcodes (Midi_HandleMetaSysex); field_8[8] is also the track data
/// pointer resolved by Midi_ResolveTrackData (absolute offset 0x74). field_2C is the
/// current track cursor advanced by the MIDI event driver (Midi_DriveTrack).
/// field_30 is a saved event cursor for looped CC 0x63. field_34 is the
/// remaining delta-time for the next event; field_38 is a fractional tick
/// accumulator (mod 6000/3600 per Display_State.region).
typedef struct _MidiTrack {
    /* 0x00 */ u8  field_0;
    /* 0x01 */ u8  field_1;
    /* 0x02 */ u8  field_2;
    /* 0x03 */ u8  field_3;
    /* 0x04 */ u8  field_4;
    /* 0x05 */ u8  field_5;
    /* 0x06 */ u8  field_6;
    /* 0x07 */ s8  field_7;
    /* 0x08 */ u8* field_8[9];
    /* 0x2C */ u8* field_2C;
    /* 0x30 */ u8* field_30;
    /* 0x34 */ s32 field_34;
    /* 0x38 */ s32 field_38;
} MidiTrack;
STATIC_ASSERT_SIZEOF(MidiTrack, 0x3C);

/// Active SPU voice slot inside MidiSong (stride 0xC, 18 slots at 0x504).
/// field_0 is the voice index (negative when free); iterated by Midi_KeyOffVoices.
/// field_0 / field_1 are set to -1 when the slot is cleared (Midi_InitSlot).
/// field_1 / field_2 match opcode nibble / param in Midi_KeyOffChannel.
/// field_3 indexes D_80068E78 for velocity scaling (Midi_UpdateVoiceVolumes).
/// field_4 is a signed per-note volume scale; field_5 is a signed pan offset.
/// field_6 / field_7 index the bank note via Snd_GetNote; field_8 is scaled pitch.
/// field_A is a reverb/enable flag halfword written by the note-on handler.
typedef struct _MidiNoteSlot {
    /* 0x0 */ s8  field_0;
    /* 0x1 */ s8  field_1;
    /* 0x2 */ s8  field_2;
    /* 0x3 */ u8  field_3;
    /* 0x4 */ s8  field_4;
    /* 0x5 */ s8  field_5;
    /* 0x6 */ u8  field_6;
    /* 0x7 */ u8  field_7;
    /* 0x8 */ s16 field_8;
    /* 0xA */ s16 field_A;
} MidiNoteSlot;
STATIC_ASSERT_SIZEOF(MidiNoteSlot, 0xC);

/// 0x10-byte linear interpolator state used by LinInterp_Setup / LinInterp_Apply /
/// LinInterp_Step. Embedded at MidiSong::field_14; BSS object LinInterp_CdStream sits
/// 0x14 bytes after CdAudio_Loc.
typedef struct _LinInterp {
    /* 0x0 */ s32 field_0;
    /* 0x4 */ s32 field_4;
    /* 0x8 */ s32 field_8;
    /* 0xC */ s16 field_C;
    /* 0xE */ s16 field_E;
} LinInterp;
STATIC_ASSERT_SIZEOF(LinInterp, 0x10);

/// State block at Midi_Song (logical stride 0x5DC; BSS allocation 0x5E0).
/// field_0 is status; field_3 is the number of track entries starting at 0x4C.
/// field_4/field_5 are copied from field_6/field_7 by the per-frame driver.
/// field_8 is a scaled volume; field_C is a sentinel (0xFFFF when cleared).
/// field_10 is a data pointer; field_14 is the volume interpolator.
/// field_34 is a tempo/rate scale used by Midi_DriveTrack with field_4+field_5;
/// field_38 is accumulated song ticks advanced by that driver.
/// field_484 is a 16-entry opcode table (same layout as MidiOpcodeCtx::field_484);
/// Midi_InitChannelTable seeds each entry with 0x407F4000 / 0.
/// voiceSlots holds up to 18 active SPU voice indices (field_0 = -1 when free).
typedef struct _SndBank      SndBank;
typedef struct _SndNote      SndNote;
typedef struct _SndBankGroup SndBankGroup;
typedef struct _MidiSong {
    /* 0x00 */ u8              field_0;
    /* 0x01 */ u8              field_1;
    /* 0x02 */ u8              field_2;
    /* 0x03 */ u8              field_3;
    /* 0x04 */ u8              field_4;
    /* 0x05 */ u8              field_5;
    /* 0x06 */ u8              field_6;
    /* 0x07 */ u8              field_7;
    /* 0x08 */ s16             field_8;
    /* 0x0A */ s16             field_A;
    /* 0x0C */ s32             field_C;
    /* 0x10 */ void*           field_10;
    /* 0x14 */ LinInterp       field_14;
    /* 0x24 */ u8              unknown_24[0x10];
    /* 0x34 */ s32             field_34;
    /* 0x38 */ s32             field_38;
    /* 0x3C */ s32             field_3C;
    /* 0x40 */ SndBank*        field_40;
    /* 0x44 */ SndBankGroup*   field_44;
    /* 0x48 */ SndNote*        field_48;
    /* 0x4C */ MidiTrack       entries[1];
    /* 0x88 */ u8              unknown_88[0x484 - 0x88];
    /* 0x484 */ MidiOpcodeSlot field_484[16];
    /* 0x504 */ MidiNoteSlot   voiceSlots[0x12];
} MidiSong;
STATIC_ASSERT_SIZEOF(MidiSong, 0x5DC);

/// 0x14-byte sound/note entry indexed by Snd_GetNote.
/// field_0 reverb enable; field_1 pan; field_3 volume; field_4/5 root-key pitch;
/// field_6 priority; field_8/9 MIDI key range; field_A/B bend ranges;
/// field_C/E ADSR; field_10 SPU waveform address.
struct _SndNote {
    /* 0x00 */ u8  field_0;  // reverbEnable
    /* 0x01 */ u8  field_1;  // pan
    /* 0x02 */ u8  pad_2;
    /* 0x03 */ u8  field_3;  // volume
    /* 0x04 */ u8  field_4;  // rootKey
    /* 0x05 */ u8  field_5;  // rootFine
    /* 0x06 */ u16 field_6;  // priority
    /* 0x08 */ u8  field_8;  // keyMin
    /* 0x09 */ u8  field_9;  // keyMax
    /* 0x0A */ u8  field_A;  // bendDown
    /* 0x0B */ u8  field_B;  // bendUp
    /* 0x0C */ u16 field_C;  // adsr1
    /* 0x0E */ u16 field_E;  // adsr2
    /* 0x10 */ s32 field_10; // waveAddr
};
STATIC_ASSERT_SIZEOF(SndNote, 0x14);

/// 4-byte group header at the start of a SndBank heap block.
/// field_0 = group size (prefix-summed into SndBank::field_10);
/// field_2 volume; field_3 pan.
struct _SndBankGroup {
    /* 0x0 */ u8 field_0; // size
    /* 0x1 */ u8 pad_1;
    /* 0x2 */ u8 field_2; // volume
    /* 0x3 */ u8 field_3; // pan
};
STATIC_ASSERT_SIZEOF(SndBankGroup, 0x4);

/// Sound bank header used by Snd_GetNote (and Snd_Banks entries, stride 0x20).
/// field_0 = SndBankGroup*; field_4 = SndNote*; field_8 = bank id (0xFxxx free);
/// field_B = group count; field_10 = u16* group index table; field_1C = heap.
struct _SndBank {
    /* 0x00 */ SndBankGroup* field_0; // groups
    /* 0x04 */ SndNote*      field_4; // notes
    /* 0x08 */ u16           field_8; // bankId
    /* 0x0A */ u8            field_A;
    /* 0x0B */ u8            field_B; // groupCount
    /* 0x0C */ u8            field_C;
    /* 0x0D */ byte          unknown_D[0x3];
    /* 0x10 */ u16*          field_10; // groupIndex
    /* 0x14 */ void*         field_14;
    /* 0x18 */ s32           field_18;
    /* 0x1C */ void*         field_1C; // heap
};
STATIC_ASSERT_SIZEOF(SndBank, 0x20);

/// Buffer with a 16-bit sum / ones-complement pair at the head and a payload
/// starting at offset 4. Written by `Mc_WriteBlockChecksum`, verified by `Mc_VerifyBlockChecksum`.
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

/// Callback-queue slot used by AsyncCb_Queue.entries (stride 0x14).
/// field_0 flags: bit0 active, bit1 arm, bit2 pending, bit3 result.
typedef struct _AsyncCbEntry {
    /* 0x00 */ s32  field_0;                            // flags
    /* 0x04 */ s32  field_4;                            // data
    /* 0x08 */ s32  (*field_8)(struct _AsyncCbEntry*);  // pollFn
    /* 0x0C */ void (*field_C)(struct _AsyncCbEntry*);  // doneFn
    /* 0x10 */ s32  (*field_10)(struct _AsyncCbEntry*); // errorFn
} AsyncCbEntry;
STATIC_ASSERT_SIZEOF(AsyncCbEntry, 0x14);

/// Ring buffer of 4 AsyncCbEntry callback slots (AsyncCb_Queue, size 0x54).
/// field_0 = readIdx; field_1 = writeIdx.
typedef struct _AsyncCbQueue {
    /* 0x00 */ s8           field_0; // readIdx
    /* 0x01 */ s8           field_1; // writeIdx
    /* 0x02 */ u8           pad_2[2];
    /* 0x04 */ AsyncCbEntry entries[4];
} AsyncCbQueue;
STATIC_ASSERT_SIZEOF(AsyncCbQueue, 0x54);

/// Descriptor pointed to by SndEvtFrom4::field_C and passed to SndVoice_AllocSlot.
/// field_5 is a volume scale (0-127) used by SndVoice_ApplyMasterVolume / SndScript_Exec;
/// field_6 is a pitch bias; field_7 is a candidate-count threshold; field_8 is a
/// preference key for func_80055EF8; field_C/field_E are halfword IDs matched by
/// SndVoice_ScanCandidates. Also the type of SndScript::field_4C voice-param blocks
/// (field_E bit1 gates the D_80082749 volume override).
typedef struct _SndVoiceParams {
    /* 0x00 */ u8  pad_0[5];
    /* 0x05 */ u8  field_5;
    /* 0x06 */ u8  field_6;
    /* 0x07 */ u8  field_7;
    /* 0x08 */ s16 field_8;
    /* 0x0A */ u8  pad_A[2];
    /* 0x0C */ u16 field_C;
    /* 0x0E */ u16 field_E;
} SndVoiceParams;
STATIC_ASSERT_SIZEOF(SndVoiceParams, 0x10);

/// Context pointed to by SndScript::field_44 (set from SndScript_Play arg4).
/// field_0 is the raw script/data base used for oneC offset tables and oneA
/// lookups; field_4 is the default sound bank when a oneV command has bank id 0.
typedef struct _SndScriptCtx {
    /* 0x0 */ u8*      field_0;
    /* 0x4 */ SndBank* field_4;
} SndScriptCtx;
STATIC_ASSERT_SIZEOF(SndScriptCtx, 0x8);

/// "oneV" (0x56656E6F) voice-on script command consumed by SndScript_Exec.
/// Also the 0x18-byte payload after a "oneC" (0x43656E6F) command.
typedef struct _SndOneV {
    /* 0x00 */ s32 magic;
    /* 0x04 */ u16 field_4;  // bank id for Snd_FindBank (0 = use ctx bank)
    /* 0x06 */ u8  field_6;  // note group for Snd_GetNote
    /* 0x07 */ u8  field_7;  // note index for Snd_GetNote
    /* 0x08 */ u16 field_8;  // duration (high half of field_8 timer units)
    /* 0x0A */ u16 field_A;  // voice countdown (0 → 0x7FFFFFFF)
    /* 0x0C */ s8  field_C;  // pan bias (<0 → use SndNote::field_1)
    /* 0x0D */ s8  field_D;  // volume scale (<0 → use SndNote::field_3)
    /* 0x0E */ s8  field_E;  // reverb gate vs D_8008274B
    /* 0x0F */ u8  pad_F;
    /* 0x10 */ u16 field_10; // voice-alloc priority for SndVoice_Alloc
    /* 0x12 */ s16 field_12; // oneA offset for SndScript_FindOneA
    /* 0x14 */ u16 field_14; // base pitch
    /* 0x16 */ s16 field_16; // oneE offset for SndVoice_SetupEnvelope (-1 disables)
} SndOneV;
STATIC_ASSERT_SIZEOF(SndOneV, 0x18);

/// "Loop" (0x706F6F4C) / "Wait" (0x74696157) / "endL" (0x4C646E65) script cmds.
/// Loop: field_4 = repeat count, field_6 = min wait; Wait: field_4 as s32 duration.
typedef struct _SndScriptCmd {
    /* 0x0 */ s32 magic;
    /* 0x4 */ u8  field_4;
    /* 0x5 */ u8  pad_5;
    /* 0x6 */ u16 field_6;
} SndScriptCmd;
STATIC_ASSERT_SIZEOF(SndScriptCmd, 0x8);

/// 0x60-byte slot in SndScript_Slots[8]. field_0 is an ID looked up by
/// SndVoice_FindById; field_16 holds status flags (mask 0xA3 selects active entries).
/// field_E is a dirty flag; field_10/11/12 and field_13/14/15 are paired ramps
/// (current/target/step) updated by SndVoice_SetPanRamp and SndVoice_SetVolumeRamp respectively.
/// field_17/field_18/field_20 are a loop stack (depth, remaining counts, restart
/// positions) used by Loop/endL in SndScript_Exec.
/// field_40 is the head of a SndVoice voice list (cleared/walked by SndScript_Play);
/// field_44 is a SndScriptCtx* script base; field_48 is the current script cursor;
/// field_F is bit1 of SndVoiceParams::field_E.
/// field_4C is a voice-param block (volume scale at field_5) walked with field_40.
/// field_50 is a volume interpolator driven by SndVoice_FadeMatching via LinInterp_Setup.
typedef struct _SndScript {
    /* 0x00 */ s32             field_0;
    /* 0x04 */ s32             field_4;
    /* 0x08 */ s32             field_8;
    /* 0x0C */ s8              field_C;
    /* 0x0D */ s8              field_D;
    /* 0x0E */ s8              field_E;
    /* 0x0F */ s8              field_F;
    /* 0x10 */ u8              field_10;
    /* 0x11 */ u8              field_11;
    /* 0x12 */ s8              field_12;
    /* 0x13 */ u8              field_13;
    /* 0x14 */ u8              field_14;
    /* 0x15 */ s8              field_15;
    /* 0x16 */ u8              field_16;
    /* 0x17 */ u8              field_17;
    /* 0x18 */ u8              field_18[8];
    /* 0x20 */ SndScriptCmd*   field_20[8];
    /* 0x40 */ SndVoice*       field_40;
    /* 0x44 */ SndScriptCtx*   field_44;
    /* 0x48 */ SndScriptCmd*   field_48;
    /* 0x4C */ SndVoiceParams* field_4C;
    /* 0x50 */ LinInterp       field_50;
} SndScript;
STATIC_ASSERT_SIZEOF(SndScript, 0x60);

/// "oneA" (0x41656E6F) tagged chunk header read by SndScript_FindOneA.
/// Located at a signed byte offset into a raw buffer.
typedef struct _SndOneA {
    /* 0x0 */ s32 field_0;
    /* 0x4 */ u16 field_4;
    /* 0x6 */ u16 field_6;
} SndOneA;
STATIC_ASSERT_SIZEOF(SndOneA, 0x8);

/// Destination for SndScript_FindOneA: receives halfwords from a SndOneA chunk.
typedef struct _SndOneAOut {
    /* 0x00 */ u8  pad_00[0x3A];
    /* 0x3A */ u16 field_3A;
    /* 0x3C */ u16 field_3C;
} SndOneAOut;
STATIC_ASSERT_SIZEOF(SndOneAOut, 0x3E);

/// 0x18-byte voice-slot lookup result filled by SndVoice_ScanCandidates and consumed by
/// SndVoice_AllocSlot / func_80055EF8. field_0 is the chosen slot index (or error);
/// field_1..field_6 are candidate slot indices (-1 = empty); field_7 is the
/// candidate count; field_8/C/10/14 hold ranking scores / IDs.
typedef struct _SndVoicePick {
    /* 0x00 */ s8  field_0;
    /* 0x01 */ s8  field_1;
    /* 0x02 */ s8  field_2;
    /* 0x03 */ s8  field_3;
    /* 0x04 */ s8  field_4;
    /* 0x05 */ s8  field_5;
    /* 0x06 */ s8  field_6;
    /* 0x07 */ u8  field_7;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
    /* 0x10 */ s32 field_10;
    /* 0x14 */ s32 field_14;
} SndVoicePick;
STATIC_ASSERT_SIZEOF(SndVoicePick, 0x18);

/// 0xC-byte init-table entry at Snd_BankInitTable (two entries used by Snd_InitBanks).
/// field_0 indexes D_800680AC for a slot id; field_2 is written to SndBankSlot.field_8
/// and SndBank.field_8; field_4/field_6 are SndHeap_Malloc sizes; field_8 is stored
/// to SndBankSlot.field_C.
typedef struct _SndBankInitEntry {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
    /* 0x4 */ u16 field_4;
    /* 0x6 */ u16 field_6;
    /* 0x8 */ s32 field_8;
} SndBankInitEntry;
STATIC_ASSERT_SIZEOF(SndBankInitEntry, 0xC);

// --- APIs (from unknown_syms) ---
void           Snd_ApplyVolumeTable(s32 arg0);
void           Spu_WaitDma(void);
void           Audio_IrqFrameWork(void);
SndBank*       Snd_AllocBank(SndBankPayload* arg0);
void           SndHeap_Reset(void);
void*          SndHeap_Malloc(size_t);
void           SndHeap_Free(void* ptr);
void           Snd_FreeBank(SndBank* arg0);
SndBank*       Snd_FindBank(u16 arg0);
void           Snd_BuildGroupIndex(SndBank* arg0);
void           LinInterp_Setup(LinInterp* arg0, s32 arg1, s32 arg2, s32 arg3);
s32            LinInterp_Apply(LinInterp* arg0, s32 arg1);
void           LinInterp_Step(LinInterp* arg0);
void           Spu_ApplyPanVolume(s16* arg0, s16 arg1, s32 arg2);
s32            Spu_TimerReentryWork(void);
void           AudioTick_Process(void);
AudioTickNode* AudioTick_Remove(AudioTickNode* arg0);
void           AudioTick_Reset(void);
void           AsyncCb_Cancel(s32 arg0);
s32            Spu_AllocVoice(s16* arg0, s32 arg1, s32 arg2);
void           Spu_SetVoiceCallbacks(u32 voiceIdx, s32 arg1, s32 arg2);
s32            Spu_SetVoiceRange(s32 idx, s32 arg1, s32 arg2);
s32            Spu_GetVoiceRef(s8 arg0, SpuVoiceRef* arg1);
u8             Spu_GetVoiceStatus(u32 voiceIdx);
void           Spu_ClearVoiceCallbacks(u32 voiceIdx);
void           Spu_KeyOff(u32 voiceIdx);
u16            Spu_CalcVolume(s32 arg0, s32 arg1, s32 arg2, s32 arg3);
SndNote*       Snd_GetNote(SndBank* arg0, u8 arg1, u8 arg2);
void           Spu_FlushVoiceUpdates(void);
s32            Spu_ReleaseVoiceSlot(u32 voiceIdx);
void           Spu_QueryReverbVoices(void);
void           Spu_ConfigReverb(s32 mode);
void           Spu_SetReverbDepth(s16 depth);
void           Spu_SetReverbMode(u32 mode);
void           Spu_EnableReverbVoice(u32 voiceIdx);
void           Spu_DisableReverbVoice(u32 voiceIdx);
bool           Spu_ReverbVoiceIsEnabled(u32 voiceIdx);
void           Spu_ApplyReverbConfig(void);
void           SndEvt_Process(void);
SndEvt*        SndEvt_Alloc(void);
s32            SndEvt_EnqueueType1(s32 arg0, s32 arg1);
s32            SndEvt_EnqueueType2(s32 arg0, s32 arg1);
s32            Midi_IsBusy(s32 arg0);
s32            Midi_IsChannelFree(u8 arg0);
void           SndEvt_Enqueue(SndEvt* arg0);
void           SndEvt_Free(SndEvt* arg0);
void           SndEvt_HandleInitSequence(SndEvt* arg0);
void           SndEvt_HandleStartFadeOut(SndEvt* arg0);
s32            Midi_InitSequence(u8 arg0, u16 arg1);
void           SndEvt_HandleFadeOn(SndEvt* arg0);
void           SndEvt_HandleFadeOff(SndEvt* arg0);
void           SndEvt_HandleSetVolume(SndEvt* arg0);
void           SndEvt_HandleAllocVoice(SndEvt* arg0);
void           SndEvt_HandleType7(SndEvt* arg0);
void           SndEvt_HandleFadeMatchingOn(SndEvt* arg0);
void           SndEvt_HandleFadeMatchingOff(SndEvt* arg0);
void           SndEvt_HandlePanRamp(SndEvt* arg0);
void           Midi_StartFadeOut(u8 arg0, u16 arg1);
void           Midi_FadeVolume(u8 arg0, s32 arg1);
void           Midi_SetVolumeScale(u8 arg0, u8 arg1);
void           Midi_SetMasterVolume(s32 arg0);
s32            Midi_GetMasterVolume(void);
u8*            Midi_GetSlot(s32 arg0);
void*          Midi_GetFixedBuffer(s32 arg0, s32 arg1);
s32            SndEvt_EnqueueType5(s32 arg0, s32 arg1);
void           SndEvt_EnqueueType5Pending(void);
void           SndEvt_FlushType5Pending(void);
void           Midi_InitSlot(s32 arg0);
void*          Midi_ResolveTrackData(MidiSong* arg0, s32 arg1, u8* arg2);
void           Midi_ResetTrackFlags(MidiSong* arg0);
void           Midi_KeyOffVoices(MidiSong* arg0);
void           Midi_DriveTrack(MidiSong* arg0, MidiTrack* arg1);
void           Midi_UpdateVoiceVolumes(MidiSong* arg0);
void*          SndLoad_AllocBuffer(s32 arg0, s32 arg1, u32 arg2);
s32            SndLoad_LookupMode(s32 arg0, s32 arg1, s32 arg2);
s32            SndLoad_ResolveSpuAddr(s32 arg0, s32 arg1);
s32            Midi_ReadVlq(u8* arg0, u8* arg1);
void           Midi_InitChannelTable(s32* arg0);
s32            Midi_IncPtr(s32 arg0, s32 arg1);
u8*            Midi_KeyOffChannel(s32 arg0, u8* arg1, MidiSong* arg2);
u8*            Midi_SetProgram(s32 arg0, u8* arg1, MidiOpcodeCtx* arg2);
u8*            Midi_PitchBend(s32 arg0, u8* arg1, MidiSong* arg2);
s32            SndLoad_ProcessSector(s32* arg0);
s32            SndLoad_Complete(SndLoadState* arg0);
s32            SndBank_SetupFromLoad(SndLoadState* arg0);
void           SndLoad_FromSectorMode8(void* arg0);
void           SndLoad_BeginFromBuffer(u8 arg0, void* arg1);
void           SndLoad_Teardown(void);
s32            SndLoad_FeedSector(void* arg0);
s32            SndLoad_FeedSectorOrError(void* arg0);
s32            SndBank_FinalizeLoad(SndLoadState* arg0);
void           SndLoad_Init(s32 arg0, void* arg1);
void           Snd_InitFromStage(s32 arg0, s32 arg1);
s32            SndBank_RemapId();
void           Snd_SetMutedVolumes(s32 arg0);
s32            SndEvt_EnqueueType6(s32 arg0, s32 arg1, s32 arg2);
void           SndEvt_EnqueueTypeA(s32 arg0, s32 arg1, s32 arg2);
void           SndEvt_EnqueueTypeB(s32 arg0, s32 arg1);
void           SndBank_SetEnableFlags(s32 arg0, s32 arg1);
void           SndVoice_SetPriority(s8 arg0);
s32            SndVoice_HasActiveId(s32 arg0);
void           SndEvt_EnqueueTypeD(void);
void           SndEvt_EnqueueTypeE(void);
void           SndEvt_EnqueueTypeF(void);
void           SndVoice_StepMasterLevel(void);
void           SndVoice_KeyOffMatching(void);
s32            SndScript_Exec(SndScript* arg0);
void           SndVoice_TickEnvelope(SndVoice* arg0);
s32            SndVoice_AllocSlot(s32 arg0, s8 arg1, s8 arg2, s32 arg3, SndVoiceParams* arg4);
void           SndVoice_ScanCandidates(SndVoicePick* arg0, u16 arg1, s32 arg2, u16 arg3);
void           SndScript_Play(s32 arg0, s8 arg1, s8 arg2, s32 arg3, s32 arg4, SndVoiceParams* arg5);
s32            SndVoice_Tick(SndVoice* arg0);
s32            SndScript_StopMatching(s32 arg0, s32 arg1);
void           SndVoice_FadeMatching(s32 arg0, s32 arg1);
void           SndVoice_SetPanRamp(s32 arg0, s32 arg1, s32 arg2);
void           SndVoice_SetVolumeRamp(s32 arg0, s32 arg1);
void           SndVoice_IncRefCount(void);
void           SndVoice_TickRefCount(void);
void           SndVoice_SetPriorityLevel(s8 arg0);
s32            SndVoice_FindById(s32 arg0);
void           SndVoice_ApplyMasterVolume(s8 arg0);
s8             SndVoice_GetMasterVolume(void);
void           SndVoice_Detach(SndVoice* arg0);
SndBankSlot*   SndBankSlot_Find(u16 arg0, s32 arg1);
SndBankSlot*   SndBankSlot_Get(s32 arg0);
void           SndBankSlot_Free(s32 arg0);
SndVoice*      SndVoice_Alloc(s32 arg0);
void           SndVoice_Attach(SndVoiceOwner* arg0, SndVoice* arg1);
s32            SndScript_TickVoices(SndScript* arg0);
void           SndVoice_ScaleVolume(s8 arg0, s8 arg1, SndVoice* arg2, LinInterp* arg3, s16* arg4);
void           SndVoice_SetupEnvelope(SndVoice* arg0, s16 arg1, u32 arg2, SndNote* arg3);
s32            SndScript_FindOneA(u8* arg0, s16 arg1, SndOneAOut* arg2);
void           SndVoice_ClearActive(void);
void           SndEvt_EnqueueType7(s32 arg0, s32 arg1);
void           Spu_InitSystem(s32 arg0);
void           Spu_Init(void);
long           Spu_TimerCallback(void);
void           Snd_ClearBanks(void);
void           AsyncCb_Poll(void);
void           AsyncCb_Reset(void);
void           Spu_InitVoices(void);
void           AudioTick_Insert(void*, u32, u32, s32*);
void           SndEvt_Reset(void);
s32            Midi_InitSystem(u32);
s32            Midi_Tick(void);
void           Snd_PollAsync(void);
void           Snd_RegisterTickCallbacks(void);
s32            SndVoice_DriveSlots(void);
s32            Snd_ReverbWarmupCb(s32* arg0);
s32            Snd_InitBanks(u32);
void           Spu_ResetCommonAttr(void);

typedef u8*        (*MidiHandler)(s32, u8*, MidiSong*, MidiTrack*);
extern MidiHandler D_800689C4[];
extern void        (*SndEvt_Handlers[])(SndEvt*);

extern HeapBlockHeader* SndHeap_Start;
extern u8               SndHeap_Buffer[SNDHEAP_SIZE];
extern AudioTickNode    AudioTick_List;
extern u32              AudioTick_Enabled;
extern long             D648E0_SpuTimerED;
extern AsyncCbQueue     AsyncCb_Queue;
extern SpuVoiceState    Spu_VoiceState;
extern SpuLVoiceTable   Spu_LVoiceTable;
extern SpuVoiceRange    Spu_VoiceRanges[];
extern u32              Spu_KeyOnMask;
extern u32              Spu_KeyOnMaskExtra;
extern u32              Spu_KeyOffMask;
extern SpuReverbConfig  Spu_ReverbCfg;
extern s32              SndEvt_Lock;
extern SndEvt*          SndEvt_Head;
extern SndEvt*          SndEvt_Tail;
extern SndEvt           SndEvt_Pool[0x40];
extern MidiSong         Midi_Song;
extern SndBank          Snd_Banks[];
extern SndBankSlot      SndBank_Slots[16];
extern SndScript        SndScript_Slots[8];
extern SndLoadState     SndLoad_State;
extern SndBankInitEntry Snd_BankInitTable[];
extern LinInterp        LinInterp_CdStream;
extern volatile u8      D_80082120;
extern u8               D58028_SpuTimerEnabled;

#endif // SOUND_H
