#ifndef GAME_H
#define GAME_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libspu.h>

/// Size of the game heap.
#define G_HEAP_SIZE 0xFF80

// It seems that the scratch space is used by the game as a temporary arena.
// At least for the bottom part. The last word of the scratch points to the
// current head, i.e., the arena grows downward.
#define G_SCRATCH_HEAD         PSX_SCRATCH_ADDR(0x3FC)
#define GameResetScratchHead() *(void**)G_SCRATCH_HEAD = G_SCRATCH_HEAD

// Module headers for renamed subsystems (types + public APIs).
#include "main/task.h"
#include "main/display.h"
#include "main/pad.h"
#include "main/mc.h"
#include "main/boot.h"
#include "main/gamemain.h"
// CdStream types are defined below; APIs in main/cdstream.h.

// CdCmdEntry / CdCmdQueue live in main/fs.h

/// CD audio player phase machine (CdAudio_Phase). Driven by switches in 46FE4.c.
typedef struct _CdAudioPhase {
    /* 0x0 */ u8 field_0; // primary phase
    /* 0x1 */ u8 field_1; // sub-phase
    /* 0x2 */ u8 field_2; // sub-phase / gate
    /* 0x3 */ u8 field_3; // stream sub-phase
    /* 0x4 */ u8 field_4; // seek / load sub-phase
    /* 0x5 */ u8 field_5; // control / abort flag
} CdAudioPhase;
STATIC_ASSERT_SIZEOF(CdAudioPhase, 0x6);

/// WIP: boot/gamemain flag block (Wip_SysFlags). field_4 set on soft-reset paths;
/// field_6 polled/cleared in boot and F344. Role not fully proven.
typedef struct _WipSysFlags {
    byte unknown_0[0x4];
    s16  field_4;
    s16  field_6;
    byte unknown_8[0x18];
} WipSysFlags;
STATIC_ASSERT_SIZEOF(WipSysFlags, 0x20);

typedef struct _HeapBlockHeader {
    u32                      size;
    u16                      isAllocated;
    u16                      magic;
    struct _HeapBlockHeader* prev;
    struct _HeapBlockHeader* next;
} HeapBlockHeader;
STATIC_ASSERT_SIZEOF(HeapBlockHeader, 0x10);

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
    /* 0x00 */ s32 field_0;  // poll: s32 (*)(s32) — return -1 to remove
    /* 0x04 */ s32 field_4;  // remove callback: void (*)(void)
    /* 0x08 */ s16 field_8;  // node id (match key)
    /* 0x0C */ s32 field_c;  // arg to poll
    /* 0x10 */ s32 field_10; // prev link
    /* 0x14 */ s32 field_14; // next link
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

// FsImgBuffers / FsWorkEntry / FsLoadParams live in main/fs.h

/// 4-byte block assigned via unaligned lwl/lwr (see Snd_ApplyVolumeTable).
typedef struct _GBytes4 {
    u8 data[4];
} GBytes4;

/// 6-byte block assigned via unaligned lwl/lwr + lb/sb (see func_80053BF4).
typedef struct _GBytes6 {
    u8 data[6];
} GBytes6;

/// 8-byte block assigned via unaligned lwl/lwr (see GameFlow_CopySaveIds).
typedef struct _GBytes8 {
    u8 data[8];
} GBytes8;

/// Overlay of objects with an 8-byte field at offset 0x4 (GameSession, McSaveData).
typedef struct _SessionBytesAt4 {
    byte    pad[4];
    GBytes8 field_4;
} SessionBytesAt4;
STATIC_ASSERT_SIZEOF(SessionBytesAt4, 0xC);

typedef struct _GameSession {
    byte  unknown_0[0x2];
    u8    field_2;
    byte  unknown_3;
    byte  field_4; // address taken by CdCmd_BuildVlcIfStream
    byte  unknown_5;
    u8    field_6;
    u8    field_7;
    byte  unknown_8[0x4];
    void* field_C[16]; // 0xC..0x4B; cleared by Game_ClearPtrSlots
    u8    field_4C;
    u8    field_4D;
    u16   field_4E; // set to 1 by Fs_LoadFile for category-8 file ids
    byte  unknown_50[0x24];
    u8    field_74;
    byte  unknown_75[0x7];
    s16   field_7C;
    s16   field_7E;
    s16   field_80;
    byte  unknown_82[0xBA];
} GameSession;
STATIC_ASSERT_SIZEOF(GameSession, 0x13C);

/// Overlay of `GameSession` starting at offset 0x4 (`field_4`..`field_7`).
/// Used when the compiler keeps `&Game_Session->field_4` in a register.
typedef struct _GameSessionFrom4 {
    /* 0x0 */ u8 field_0; // GameSession.field_4
    /* 0x1 */ u8 field_1; // GameSession.unknown_5
    /* 0x2 */ u8 field_2; // GameSession.field_6
    /* 0x3 */ u8 field_3; // GameSession.field_7
} GameSessionFrom4;
STATIC_ASSERT_SIZEOF(GameSessionFrom4, 0x4);

/// Large object pointed to by Task::field_1C for the slot-3 game object
/// (Game_GetPtrSlot(3)). Sparse fields used by Display_SpawnFromMode.
typedef struct _GameActor {
    /* 0x000 */ byte pad_0[0x90];
    /* 0x090 */ s32  field_90;
    /* 0x094 */ byte pad_94[0xE8];
    /* 0x17C */ byte field_17C; // address taken for func_800E1A6C
    /* 0x17D */ byte pad_17D[0x7B3];
    /* 0x930 */ byte field_930; // address taken for func_801011D0
    /* 0x931 */ byte pad_931[0x53];
    /* 0x984 */ u8   field_984;
} GameActor;
STATIC_ASSERT_SIZEOF(GameActor, 0x988);

/// Object pointed to by Task::field_2c; field_8 is a s32* cleared by
/// Display_SpawnFromMode after optional func_801011D0 / func_800E1A6C setup.
/// field_C flag bits are OR'd with 0x80 in Task_Kill (type-1 deferred kill).
typedef struct _GameActorExt {
    /* 0x0 */ byte pad_0[0x8];
    /* 0x8 */ s32* field_8;
    /* 0xC */ u16  field_C;
    /* 0xE */ byte pad_E[0x2];
} GameActorExt;
STATIC_ASSERT_SIZEOF(GameActorExt, 0x10);

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
    /* 0x00 */ s16             field_0; // allocated (0 free, 1 in use)
    /* 0x02 */ s16             field_2; // SndEvt_Handlers index
    /* 0x04 */ u8              field_4; // handler payload
    /* 0x05 */ u8              field_5;
    /* 0x06 */ u16             field_6;
    /* 0x08 */ s32             field_8;
    /* 0x0C */ s32             field_C;
    /* 0x10 */ s32             field_10;
    /* 0x14 */ struct _SndEvt* field_14; // queue prev
    /* 0x18 */ struct _SndEvt* field_18; // queue next
} SndEvt;
STATIC_ASSERT_SIZEOF(SndEvt, 0x1C);

/// Stage / flow context (Stage_Ctx → bss D_8007A320, size 0x38).
typedef struct _StageCtx {
    /* 0x00 */ TaskDesc* field_0; // task desc table for spawn
    /* 0x04 */ s32       field_4; // spawn arg
    /* 0x08 */ s32       field_8; // spawn arg
    /* 0x0C */ u32       field_C;
    /* 0x10 */ byte      unknown_10;
    /* 0x11 */ u8        field_11;
    /* 0x12 */ u8        field_12; // flow gate
    /* 0x13 */ u8        field_13;
    /* 0x14 */ u8        field_14;
    /* 0x15 */ u8        field_15;
    /* 0x16 */ byte      unknown_16;
    /* 0x17 */ u8        field_17; // flow gate
    /* 0x18 */ u8        field_18;
    /* 0x19 */ u8        field_19; // flag bits (bit0/1)
    /* 0x1A */ u8        field_1a;
    /* 0x1B */ byte      unknown_1b;
    /* 0x1C */ u32       field_1c;    // flag word
    /* 0x20 */ s32       field_20;
    /* 0x24 */ s32       field_24;    // last Display_State.field_118
    /* 0x28 */ s32       field_28;    // step counter
    /* 0x2C */ u8        field_2C[8]; // CDF load param block
    /* 0x34 */ u8        field_34[4]; // CDF load param block
} StageCtx;
STATIC_ASSERT_SIZEOF(StageCtx, 0x38);

/// BSS object CdAudio_Loc (size 0x10). Sector / pitch state for CD audio player.
typedef struct _CdAudioLoc {
    /* 0x0 */ u8  field_0; // status
    /* 0x1 */ u8  field_1; // ready / done flag
    /* 0x2 */ u16 field_2; // pitch-related
    /* 0x4 */ s32 field_4; // sector / position
    /* 0x8 */ s32 field_8; // SPU / buffer param
    /* 0xC */ s32 field_C; // sector position
} CdAudioLoc;
STATIC_ASSERT_SIZEOF(CdAudioLoc, 0x10);

/// Sector payload pointed to by CdStreamState::sector (MTS audio stream sector).
typedef struct _MtsSector {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ u32 magic; // high 3 bytes = "MTS", low byte = channel count
    /* 0x0C */ s8  field_C;
    /* 0x0D */ u8  field_D;
    /* 0x0E */ u8  field_E;
    /* 0x0F */ s8  field_F;
} MtsSector;

/// BSS object CdStream_State (size 0x58). CD/SPU stream runtime state.
typedef struct _CdStreamState {
    /* 0x00 */ u8         flags0; // bit0 busy, bit1 voice-on, bit3 IRQ, bit4 voices alloc, bit6 stop
    /* 0x01 */ u8         flags1; // bit0 voices started, bit1 ending, bit3 param pending, bit7 enable param
    /* 0x02 */ u8         flags2; // bit0 continue-arm, bit1/2 stream phase, bit3 XA pause
    /* 0x03 */ u8         phase;  // 1 = completing, 2 = streaming
    /* 0x04 */ u8         field_4;
    /* 0x05 */ u8         pad_5;
    /* 0x06 */ s16        readySlot; // 1-based CdReady_Queue index, 0 = none
    /* 0x08 */ void       (*doneCb)(s32);
    /* 0x0C */ void       (*startCb)(s32);
    /* 0x10 */ void       (*voiceFreeCb)(s32);
    /* 0x14 */ s32        field_14;
    /* 0x18 */ s32        field_18;
    /* 0x1C */ s32        field_1C;
    /* 0x20 */ s32        field_20;
    /* 0x24 */ s32        spuAddr;
    /* 0x28 */ s32        startSector;
    /* 0x2C */ s32        field_2C;
    /* 0x30 */ s32        field_30;
    /* 0x34 */ s32        field_34;
    /* 0x38 */ s32        field_38;
    /* 0x3C */ s32        spuBase;
    /* 0x40 */ s16        sectorsPerChunk; // 0x18 (NTSC) or 0x14 (PAL)
    /* 0x42 */ s16        ringHalf;        // 0x2770; half ring used in SPU addr math
    /* 0x44 */ s16        countdown;
    /* 0x46 */ s8         mtsPeriod;       // divisor for remaining % period SpuWrite cadence
    /* 0x47 */ u8         mtsParam;
    /* 0x48 */ MtsSector* sector;
    /* 0x4C */ s16        field_4C;
    /* 0x4E */ s16        remaining;
    /* 0x50 */ u8         voiceL;
    /* 0x51 */ u8         voiceR;
    /* 0x52 */ s8         mode;
    /* 0x53 */ u8         flags;         // bit1 = linked L/R pitch
    /* 0x54 */ u16        pending;
    /* 0x56 */ u16        settleCounter; // disc init settle ticks
} CdStreamState;
STATIC_ASSERT_SIZEOF(CdStreamState, 0x58);

/// One half of CdStream_Channels (stride 0x40). Dual L/R audio channel state.
/// Immediately follows CdStream_State in BSS (CdStream_Start treats it as
/// (CdStreamChannel*)(&CdStream_State + 1)).
typedef struct _CdStreamChannel {
    /* 0x00 */ s32  voiceMask;
    /* 0x04 */ s32  attr; // Spu voice attribute bitfield (key-on bits etc.)
    /* 0x08 */ s16  pitch;
    /* 0x0A */ s16  pitchAlt;
    /* 0x0C */ s16  field_C;
    /* 0x0E */ s16  field_E;
    /* 0x10 */ byte pad_10[4];
    /* 0x14 */ s16  field_14; // often 0x1000
    /* 0x16 */ byte pad_16[6];
    /* 0x1C */ s32  spuAddr;
    /* 0x20 */ s32  spuAddr2;
    /* 0x24 */ byte pad_24[0x16];
    /* 0x3A */ s16  field_3A;
    /* 0x3C */ s16  field_3C;
    /* 0x3E */ byte pad_3E[2];
} CdStreamChannel;
STATIC_ASSERT_SIZEOF(CdStreamChannel, 0x40);

/// BSS object CdStream_Channels (size 0x80). Two CdStreamChannel channels at +0x00 / +0x40.
typedef struct _CdStreamChannels {
    /* 0x00 */ CdStreamChannel ch[2];
} CdStreamChannels;
STATIC_ASSERT_SIZEOF(CdStreamChannels, 0x80);

/// One slot in CdReady_Queue.entries (stride 0x14).
/// flags: bit0 active, bit1 armed, bit2 cancel/pending, bit3 result.
typedef struct _CdReadyEntry {
    /* 0x00 */ u32 flags;
    /* 0x04 */ s32 sectorPos;
    /* 0x08 */ s32 pollFn;  // s32 (*)(CdReadyEntry*)
    /* 0x0C */ s32 doneFn;  // void (*)(void) when complete
    /* 0x10 */ s32 errorFn; // void (*)(CdReadyEntry*) on cancel path
} CdReadyEntry;
STATIC_ASSERT_SIZEOF(CdReadyEntry, 0x14);

/// BSS object CdReady_Queue (size 0x58). Ring of CD ready work items + callback state.
typedef struct _CdReadyQueue {
    /* 0x00 */ u8           locked;            // re-entrancy guard
    /* 0x01 */ u8           callbackInstalled; // CdReadyCallback currently ours
    /* 0x02 */ u8           readIdx;
    /* 0x03 */ u8           writeIdx;
    /* 0x04 */ void*        prevCallback; // previous CdReadyCallback
    /* 0x08 */ CdReadyEntry entries[4];
} CdReadyQueue;
STATIC_ASSERT_SIZEOF(CdReadyQueue, 0x58);

/// Object passed to Ui_UpdateListNoAnim / Ui_SmoothCursor (e.g. via Task::field_20).
typedef struct _UiMiniObj {
    /* 0x00 */ s32  field_0;
    /* 0x04 */ byte unknown_4[0x18];
    /* 0x1C */ s16  field_1c;
    /* 0x1E */ s16  unknown_1e;
    /* 0x20 */ s16  field_20;
    /* 0x22 */ s16  field_22;
} UiMiniObj;
STATIC_ASSERT_SIZEOF(UiMiniObj, 0x24);

/// Object at Task::field_20 used by func_80048838 / Mc_HideChildUi /
/// Mc_DrawPrompt / func_800486F0. Shares the UiPanel layout through offset
/// 0x24 (handlers cast field_20 to UiPanel*). field_0 is a status flag;
/// field_4 is copied from UiObjectDesc::field_0 at spawn; field_8 is a mode
/// (5 = skip draw in Text_DrawPrompt / Text_DrawMultiLine; set to 3 when torn down); field_C..field_12
/// are layout halfwords (RECT-like); field_14 is a halfword counter used as the
/// text draw priority/order; field_16 is a signed timer/counter; field_18/field_1A
/// are layout offsets (shared with UiPanel; used when positioning child UI);
/// field_1C is a position halfword (+2 when passed to Text_DrawPrompt); field_1E is
/// an x offset paired with field_20; field_20/field_22 are base x/y for relative
/// text placement; field_24 is a callback copied from the descriptor; field_28 is
/// the owning Task*; field_2C / field_2E are halfwords polled by teardown state
/// handlers (e.g. GameFlow_WaitMenuDone waits until field_2E == -1 before cleaning up;
/// dialog pickers set field_2E == 6 when a choice is confirmed).
typedef struct _UiObject {
    /* 0x00 */ s32   field_0; // status flag
    /* 0x04 */ s32   field_4; // from UiObjectDesc::field_0
    /* 0x08 */ s32   field_8; // mode (5=skip draw, 3=torn down)
    /* 0x0C */ u16   field_C; // layout (RECT-like)
    /* 0x0E */ u16   field_E;
    /* 0x10 */ u16   field_10;
    /* 0x12 */ u16   field_12;
    /* 0x14 */ u16   field_14; // text draw priority/order
    /* 0x16 */ s16   field_16; // timer/counter
    /* 0x18 */ u16   field_18; // layout offset
    /* 0x1A */ u16   field_1A; // layout offset
    /* 0x1C */ s16   field_1C; // position (+2 for text draw)
    /* 0x1E */ u16   field_1E; // x offset with field_20
    /* 0x20 */ u16   field_20; // base x
    /* 0x22 */ u16   field_22; // base y
    /* 0x24 */ s32   field_24; // callback (from desc)
    /* 0x28 */ Task* field_28; // owning task
    /* 0x2C */ s16   field_2C; // teardown / choice
    /* 0x2E */ s16   field_2E; // teardown / choice (-1 wait, 6 confirm)
} UiObject;
STATIC_ASSERT_SIZEOF(UiObject, 0x30);

/// Template/descriptor consumed by func_800486F0 to spawn a UiObject + Task.
typedef struct _UiObjectDesc {
    /* 0x00 */ s32 field_0; // → UiObject.field_4
    /* 0x04 */ u16 field_4; // → layout
    /* 0x06 */ u16 field_6;
    /* 0x08 */ u16 field_8;
    /* 0x0A */ u16 field_A;
    /* 0x0C */ u16 field_C;
    /* 0x0E */ u16 field_E;
    /* 0x10 */ u16 field_10; // → TaskDesc seed
    /* 0x12 */ u16 field_12; // → TaskDesc seed
    /* 0x14 */ s32 field_14; // → UiObject callback-ish
    /* 0x18 */ s32 field_18; // → TaskDesc seed
} UiObjectDesc;
STATIC_ASSERT_SIZEOF(UiObjectDesc, 0x1C);

/// Singly-linked text line node used by TextBlockDesc / func_800480A0.
typedef struct TextLineNode {
    /* 0x0 */ u8*                  text;
    /* 0x4 */ struct TextLineNode* next;
} TextLineNode;

/// Multi-line text block descriptor consumed by func_800480A0 to spawn a
/// sized UiObject. field_0 is the line count; field_2 is cleared on return;
/// field_4 is the head of a TextLineNode list; field_8 selects layout mode
/// (0 forces UiObject::field_4 = 3).
typedef struct TextBlockDesc {
    /* 0x0 */ s16           count;
    /* 0x2 */ s16           field_2;
    /* 0x4 */ TextLineNode* lines;
    /* 0x8 */ s32           field_8;
} TextBlockDesc;
STATIC_ASSERT_SIZEOF(TextBlockDesc, 0xC);

/// 8-byte slot at MidiOpcodeCtx::field_484 (16 entries, indexed by opcode low nibble).
/// Seeded as the word 0x407F4000 by func_800528BC (field_0..field_3 little-endian);
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

/// Large context object used by 410B0.c opcode handlers (e.g. func_800529BC).
/// Only fields used so far are named; size is incomplete.
typedef struct _MidiOpcodeCtx {
    /* 0x000 */ byte           unknown_0[0x484];
    /* 0x484 */ MidiOpcodeSlot field_484[16];
} MidiOpcodeCtx;

/// Element of BSS array Stream_Slots (15 entries, total 0x258).
/// Fields inferred from F344.c accessors (Stream_InitFromSlot, Stream_FindSlotByKey, etc.).
typedef struct _StreamSlot {
    /* 0x00 */ s16  field_0;
    /* 0x02 */ byte unknown_2[0x2];
    /* 0x04 */ s32  field_4;
    /* 0x08 */ byte unknown_8[0x4];
    /* 0x0C */ u16  field_C;
    /* 0x0E */ u16  field_E;
    /* 0x10 */ u16  field_10;
    /* 0x12 */ u16  field_12;
    /* 0x14 */ u16  field_14;
    /* 0x16 */ u16  field_16;
    /* 0x18 */ u16  field_18;
    /* 0x1A */ u16  field_1A;
    /* 0x1C */ u16  field_1C;
    /* 0x1E */ u16  field_1E;
    /* 0x20 */ byte unknown_20[0x2];
    /* 0x22 */ u16  field_22;
    /* 0x24 */ u16  field_24;
    /* 0x26 */ u16  field_26;
} StreamSlot;
STATIC_ASSERT_SIZEOF(StreamSlot, 0x28);

/// UI list/menu object (data symbols D_8006116C, D_80061194, D_8006125C,
/// D_80061284, D_800612AC, D_80067654; size 0x24).
/// field_0 is a function-table pointer; field_4 / field_5 are base indices
/// (func_80049C00 seeds both from context); field_5 is also subtracted when
/// computing field_9; field_6 / field_7 are signed layout sizes (Ui_DrawListHighlight
/// uses field_7 as TILE height); field_9 / field_A / field_10 are list cursor /
/// flag / selection index used by McMenu_SelectList / McMenu_SelectListAlt / McMenu_InitByMode /
/// Ui_InitList / func_8004917C; field_C / field_14 / field_16 are cleared by
/// Ui_InitList; field_17 is a signed layout adjust subtracted from the child
/// height when computing visible rows (func_80048AEC / func_80048C30; the latter
/// also writes field_17 from its third argument).
typedef struct _UiList {
    /* 0x00 */ byte unknown_0[0x4]; // often function-table pointer
    /* 0x04 */ u8   field_4;        // base index
    /* 0x05 */ u8   field_5;        // base index (also used vs field_9)
    /* 0x06 */ s8   field_6;        // layout size
    /* 0x07 */ s8   field_7;        // TILE height / row height
    /* 0x08 */ byte unknown_8;
    /* 0x09 */ u8   field_9;        // list cursor (visible offset)
    /* 0x0A */ u8   field_A;        // flag
    /* 0x0B */ byte unknown_B;
    /* 0x0C */ s32  field_C;        // cleared by list reset
    /* 0x10 */ s32  field_10;       // selection index
    /* 0x14 */ s16  field_14;       // cleared by list reset
    /* 0x16 */ s8   field_16;       // cleared by list reset
    /* 0x17 */ s8   field_17;       // layout adjust for visible rows
    /* 0x18 */ byte unknown_18[0xC];
} UiList;
STATIC_ASSERT_SIZEOF(UiList, 0x24);

/// WIP: Task::field_34 context for D_8006121C select-menu (McMenu_SelectListAlt).
/// Only field_290 is used so far (seeds UiList cursor).
typedef struct _WipSelectMenuExt {
    /* 0x000 */ byte unknown_0[0x290];
    /* 0x290 */ s32  field_290;
} WipSelectMenuExt;

/// 4-byte entry at Spu_VoiceRanges (see Spu_SetVoiceRange).
typedef struct _SpuVoiceRange {
    /* 0x0 */ s16 field_0; // first voice index
    /* 0x2 */ s16 field_2; // count of voices in range
} SpuVoiceRange;
STATIC_ASSERT_SIZEOF(SpuVoiceRange, 0x4);

/// Source/model data pointed to by TmdObject::field_10 (see Tmd_Create).
typedef struct _TmdSource {
    /* 0x00 */ s32  field_0;  // one-shot init flag (set 1)
    /* 0x04 */ s32  field_4;  // byte count for aux alloc (calloc size * 2)
    /* 0x08 */ s32  field_8;  // offset into half-buffer base
    /* 0x0C */ byte unknown_C[0x8];
    /* 0x14 */ s32  field_14; // copied to scratch ws
    /* 0x18 */ s32  field_18; // copied to scratch ws
    /* 0x1C */ byte unknown_1C[0x4];
    /* 0x20 */ u32* field_20; // [id, handler_slot, dims, data…] stream
} TmdSource;

/// Node in the Tmd_List linked list (2F244.c TMD/model objects).
typedef struct _TmdObject {
    /* 0x00 */ struct _TmdObject* next;
    /* 0x04 */ byte               unknown_4[0x4];
    /* 0x08 */ void*              field_8;
    /* 0x0C */ u16                field_C;
    /* 0x0E */ s8                 field_E;
    /* 0x0F */ byte               unknown_F;
    /* 0x10 */ TmdSource*         field_10; // source / model data
    /* 0x14 */ u16                field_14; // cleared when buffers alloc
    /* 0x16 */ u16                field_16;
    /* 0x18 */ void*              field_18; // aux buffer (Tmd_AllocBuffers)
    /* 0x1C */ void*              field_1C;
    /* 0x20 */ void*              field_20;
    /* 0x24 */ u8                 field_24;
    /* 0x25 */ u8                 field_25;
    /* 0x26 */ byte               unknown_26[0xA];
    /* 0x30 */ s32                field_30;
} TmdObject;
STATIC_ASSERT_SIZEOF(TmdObject, 0x34);

/// Sentinel list head for TmdObject (and similar) intrusive lists.
/// Same layout as TaskNode: next is the first element, prev is the last
/// (or &self when the list is empty). Initialized by Tmd_InitLists.
typedef struct _TmdListHead {
    /* 0x00 */ TmdObject*           next;
    /* 0x04 */ struct _TmdListHead* prev;
} TmdListHead;
STATIC_ASSERT_SIZEOF(TmdListHead, 0x8);

/// Head of the TmdObject linked list used by 2F244.c TMD/model helpers.
extern TmdListHead Tmd_List;
/// Second list head initialized alongside Tmd_List by Tmd_InitLists.
extern TmdListHead Tmd_ListAlt;

/// WIP: nested object reached via WipUiHolder::field_28 (func_80049D34 writes field_34).
typedef struct _WipUiChild {
    /* 0x00 */ byte unknown_0[0x34];
    /* 0x34 */ s32  field_34;
} WipUiChild;

/// WIP: UI holder pointer Wip_UiHolder; field_28 → WipUiChild.
typedef struct _WipUiHolder {
    /* 0x00 */ byte        unknown_0[0x28];
    /* 0x28 */ WipUiChild* field_28;
} WipUiHolder;

/// Object used by 34E98.c handlers (e.g. func_80049554 / D_80013F2C table).
/// field_4 low nibble selects layout padding (Ui_InsetLayout); high nibble of the
/// low byte selects a fill mode (Ui_ScaleRect). field_8 is a small integer
/// state; field_C is a source RECT used by layout helpers (Ui_InsetLayout /
/// Ui_ComputeAnimRect); field_14 is a halfword counter temporarily adjusted around
/// text draw (func_80048F88); field_16 is a signed counter/timer;
/// field_18..field_22 are layout offsets (func_80049024 / Ui_InsetLayout);
/// field_24 is a callback invoked with the second handler argument.
typedef struct _UiPanel {
    /* 0x00 */ s32  field_0;
    /* 0x04 */ s32  field_4;  // low nibble layout pad; high nibble fill mode
    /* 0x08 */ s32  field_8;  // small integer state
    /* 0x0C */ RECT field_C;  // source RECT for layout
    /* 0x14 */ u16  field_14; // halfword counter (text draw)
    /* 0x16 */ s16  field_16; // timer/counter
    /* 0x18 */ u16  field_18; // layout offset
    /* 0x1A */ u16  field_1A; // layout offset
    /* 0x1C */ u16  field_1C; // layout (signed in some overlays)
    /* 0x1E */ u16  field_1E; // layout (signed in some overlays)
    /* 0x20 */ u16  field_20;
    /* 0x22 */ u16  field_22;
    /* 0x24 */ void (*field_24)(void*); // handler callback
} UiPanel;

/// Callback for UiPanel state handlers (e.g. entries in D_80013F2C).
typedef void (*UiPanelFunc)(UiPanel* arg0, void* arg1);

/// Fixed-size table of UiPanelFunc callbacks. Copied onto the stack by
/// func_800498D4 so the call uses a local jump table.
typedef struct {
    UiPanelFunc funcs[6];
} UiPanelFuncTable6;

/// Header for the bank table blob pointed to by SndBankSlot.field_0.
/// field_4 is the bank ID (high halfword remapped by func_80053F00 when the
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
/// Passed to Snd_AllocBank; filled from a CD sector by func_80052B30.
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

/// State block at SndLoad_State; field_3 is also D_800820F3.
/// field_14/field_18 cleared by Snd_InitFromStage; field_10 sized by SndLoad_Init.
/// field_26/field_28 set by the CD ready path in CdAudio_FeedSector.
/// field_1C..field_2C are filled as five words from a sector by func_80052B30
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

/// Per-buffer OT context (Gpu_OtBuffers[2]). Indexed by display buffer (stride 0x14).
/// field_4 is OT start; field_10 is the last tag (passed to DrawOTag).
typedef struct _GpuOtBuf {
    /* 0x00 */ s32     field_0;
    /* 0x04 */ u_long* field_4;
    /* 0x08 */ u8      unknown_08[0x8];
    /* 0x10 */ u_long* field_10;
} GpuOtBuf;
STATIC_ASSERT_SIZEOF(GpuOtBuf, 0x14);

/// Track/channel entry inside MidiSong (stride 0x3C). field_5 is a per-entry flag
/// written by func_80051AB8; absolute offset of first entry's field_5 is 0x51.
/// field_0 / field_1 / field_4 are NRPN/RPN state used by the MIDI CC handler
/// (func_80052488). field_6 / field_7 and field_8[] form a loop stack for the
/// 0xF5/0xF6 meta opcodes (func_800526A4); field_8[8] is also the track data
/// pointer resolved by func_80051A2C (absolute offset 0x74). field_2C is the
/// current track cursor advanced by the MIDI event driver (Midi_DriveTrack).
/// field_30 is a saved event cursor for looped CC 0x63. field_34 is the
/// remaining delta-time for the next event; field_38 is a fractional tick
/// accumulator (mod 6000/3600 per Display_State.field_124).
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
/// field_1 / field_2 match opcode nibble / param in func_800528F8.
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
/// func_800528BC seeds each entry with 0x407F4000 / 0.
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

/// Text-measure / draw-request block passed to Text_MeasureAndCenter / func_8002E53C.
/// field_0/field_2 = x/y (or accumulate measured width); field_4 = OT priority;
/// field_C selects Font_Glyphs0/1/2; field_D = center mode (1=half, 2=full);
/// field_F = v bias added when drawing sprites.
typedef struct _TextDrawReq {
    /* 0x00 */ s16 field_0; // x
    /* 0x02 */ s16 field_2; // y
    /* 0x04 */ s32 field_4; // otIndex
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s8  field_C; // glyphTable
    /* 0x0D */ s8  field_D; // centerMode
    /* 0x0E */ s8  field_E;
    /* 0x0F */ u8  field_F; // vBias
} TextDrawReq;
STATIC_ASSERT_SIZEOF(TextDrawReq, 0x10);

/// Per-glyph metrics in the font tables (Font_Glyphs0 / Font_Glyphs1 / Font_Glyphs2).
/// off_x / off_y are stored as bytes but used as signed offsets when drawing.
typedef struct _FontGlyph {
    /* 0x0 */ u8 u;
    /* 0x1 */ u8 v;
    /* 0x2 */ u8 w;
    /* 0x3 */ u8 h;
    /* 0x4 */ u8 off_x;
    /* 0x5 */ u8 off_y;
    /* 0x6 */ u8 field_6;
    /* 0x7 */ u8 field_7;
    /* 0x8 */ u8 field_8;
    /* 0x9 */ u8 field_9;
    /* 0xA */ u8 pad_A[2];
} FontGlyph;
STATIC_ASSERT_SIZEOF(FontGlyph, 0xC);

/// Draw params for Prim_DrawSprt (SPRT) / Prim_DrawTile (TILE).
/// field_0/2 = x/y; field_4/6 = u/v (SPRT); field_8/A = w/h (inclusive, decremented
/// when written); field_C/D/E = RGB; field_10 = 0 shade-tex / nonzero semi-trans.
typedef struct _PrimDrawParams {
    /* 0x00 */ s16 field_0;  // x
    /* 0x02 */ s16 field_2;  // y
    /* 0x04 */ u8  field_4;  // u
    /* 0x05 */ u8  pad_5;
    /* 0x06 */ u8  field_6;  // v
    /* 0x07 */ u8  pad_7;
    /* 0x08 */ s16 field_8;  // w
    /* 0x0A */ s16 field_A;  // h
    /* 0x0C */ u8  field_C;  // r
    /* 0x0D */ u8  field_D;  // g
    /* 0x0E */ u8  field_E;  // b
    /* 0x0F */ u8  pad_F;
    /* 0x10 */ s16 field_10; // shadeMode
} PrimDrawParams;
STATIC_ASSERT_SIZEOF(PrimDrawParams, 0x12);

/// 4-byte glyph UVWH entry used by TextStream_Draw (tables like D_800627E0).
/// Distinct from FontGlyph (0xC full font metrics).
typedef struct _GlyphUvwh {
    /* 0x0 */ u8 u;
    /* 0x1 */ u8 v;
    /* 0x2 */ u8 w;
    /* 0x3 */ u8 h;
} GlyphUvwh;
STATIC_ASSERT_SIZEOF(GlyphUvwh, 0x4);

/// Text stream / font draw object (e.g. D_800630B0).
/// field_0/2 = x/y; field_4/6 = tpage xy (u base = field_4 & 0x3F);
/// field_8/A = clut xy; field_C = per-char delay; field_E = stream cursor;
/// field_10 = char stream (0xFE newline, 0xFF end); field_14 = GlyphUvwh table;
/// field_18 = line height; field_1A = delay reload on line advance.
typedef struct _TextStream {
    /* 0x00 */ s16        field_0;  // x
    /* 0x02 */ s16        field_2;  // y
    /* 0x04 */ s16        field_4;  // tpageX
    /* 0x06 */ s16        field_6;  // tpageY
    /* 0x08 */ s16        field_8;  // clutX
    /* 0x0A */ s16        field_A;  // clutY
    /* 0x0C */ s16        field_C;  // charDelay
    /* 0x0E */ s16        field_E;  // cursor
    /* 0x10 */ u8*        field_10; // chars
    /* 0x14 */ GlyphUvwh* field_14; // glyphs
    /* 0x18 */ s16        field_18; // lineHeight
    /* 0x1A */ s16        field_1A; // delayReload
    /* 0x1C */ s16        field_1C;
    /* 0x1E */ s16        field_1E;
} TextStream;
STATIC_ASSERT_SIZEOF(TextStream, 0x20);

/// BSS object CdAudio_Tbl (size 0x18). CD/audio stream state for 46FE4.c.
/// field_C is a base pointer into a halfword table; CdAudio_LoadSectorEntry indexes it
/// with ((packed >> 14) & 0x3FC) / 2 (4-byte stride, low halfword of each slot).
typedef struct _CdAudioTbl {
    /* 0x00 */ u8   field_0;
    /* 0x01 */ u8   field_1;
    /* 0x02 */ u8   field_2; // index into CdAudio_TblEntries
    /* 0x03 */ u8   pad_3;
    /* 0x04 */ s32  field_4;
    /* 0x08 */ s32  field_8;
    /* 0x0C */ u16* field_C;  // halfword table base (CdAudio_LoadSectorEntry)
    /* 0x10 */ s32  field_10; // transfer / SpuWrite param
    /* 0x14 */ s32  field_14;
} CdAudioTbl;
STATIC_ASSERT_SIZEOF(CdAudioTbl, 0x18);

/// WIP: BSS Wip_SysConfig (0x80). Init by Mc_InitSaveSlotDefaults (four s16s = 100);
/// field_40 filled 0xFF by Mc_InitDualBankBuffers. Likely mix/options block — unproven.
typedef struct _WipSysConfig {
    /* 0x00 */ byte unknown_0[0x8];
    /* 0x08 */ s32  field_8;
    /* 0x0C */ byte unknown_C[0xC];
    /* 0x18 */ s16  field_18;
    /* 0x1A */ s16  field_1a;
    /* 0x1C */ s16  field_1c;
    /* 0x1E */ s16  field_1e;
    /* 0x20 */ u8   field_20;
    /* 0x21 */ u8   field_21;
    /* 0x22 */ byte unknown_22[0x4];
    /* 0x26 */ u8   field_26;
    /* 0x27 */ byte unknown_27[0x19];
    /* 0x40 */ u8   field_40[0x40];
} WipSysConfig;
STATIC_ASSERT_SIZEOF(WipSysConfig, 0x80);

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

/// BSS object CdAudio_Ctl (size 0x14). CD stream control for 46FE4.c.
typedef struct _CdAudioCtl {
    /* 0x00 */ s32 field_0;  // busy / retry counter
    /* 0x04 */ s32 field_4;  // secondary counter
    /* 0x08 */ u8  field_8;  // phase mirror
    /* 0x09 */ u8  field_9;
    /* 0x0A */ s8  field_A;  // error code (-1 / -2)
    /* 0x0B */ u8  field_B;
    /* 0x0C */ s32 field_C;  // countdown
    /* 0x10 */ s32 field_10; // control flag
} CdAudioCtl;
STATIC_ASSERT_SIZEOF(CdAudioCtl, 0x14);

/// BSS object CdStream_Params (size 0x20). CD/SPU stream setup block for
/// func_800567E4 / CdStream_Start: sector position, buffer, callbacks, and
/// voice indices.
typedef struct _CdStreamParams {
    /* 0x00 */ s32   startSector;
    /* 0x04 */ s32   spuBase;
    /* 0x08 */ void* sectorBuf;
    /* 0x0C */ void  (*doneCb)(void);
    /* 0x10 */ s32   startCb;     // stored into CdStreamState::startCb
    /* 0x14 */ s32   voiceFreeCb; // stored into CdStreamState::voiceFreeCb
    /* 0x18 */ s16   pitch;
    /* 0x1A */ s8    voiceL;
    /* 0x1B */ s8    voiceR;
    /* 0x1C */ u8    mode;
    /* 0x1D */ u8    pad_1D[3];
} CdStreamParams;
STATIC_ASSERT_SIZEOF(CdStreamParams, 0x20);

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

/// Double-buffered ordering-table descriptor (same layout as PsyQ GsOT).
/// Used by Gpu_OrderingTables and passed to GsClearOt.
typedef struct _GameOt {
    /* 0x00 */ u_long  length;
    /* 0x04 */ u_long* org;
    /* 0x08 */ u_long  offset;
    /* 0x0C */ u_long  point;
    /* 0x10 */ u_long* tag;
} GameOt;
STATIC_ASSERT_SIZEOF(GameOt, 0x14);

/// PsyQ GsClearOt, declared with GameOt* so callers need not include libgs.h.
void GsClearOt(unsigned short offset, unsigned short point, GameOt* otp);

/// Callback-queue slot used by AsyncCb_Queue.entries / AsyncCb_Entries (stride 0x14).
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
/// field_0 = readIdx; field_1 = writeIdx. AsyncCb_Entries aliases entries[0].
typedef struct _AsyncCbQueue {
    /* 0x00 */ s8           field_0; // readIdx
    /* 0x01 */ s8           field_1; // writeIdx
    /* 0x02 */ u8           pad_2[2];
    /* 0x04 */ AsyncCbEntry entries[4];
} AsyncCbQueue;
STATIC_ASSERT_SIZEOF(AsyncCbQueue, 0x54);

/// 4-byte entry pointed to by CdAudio_TblEntries (see func_80057724).
/// Indexed by CdAudio_Tbl.field_2; field_3 is compared across adjacent entries.
typedef struct _CdAudioTblEntry {
    /* 0x0 */ u8 pad[3];
    /* 0x3 */ u8 field_3; // compared across adjacent entries for span
} CdAudioTblEntry;
STATIC_ASSERT_SIZEOF(CdAudioTblEntry, 0x4);

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
    /* 0x12 */ s16 field_12; // oneA offset for func_8005664C
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
/// (current/target/step) updated by func_80055A9C and SndVoice_SetVolumeRamp respectively.
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

/// BSS block covering CdAudio_Loc (0x10) + D_800827B0 (0x4). Immediately precedes
/// LinInterp_CdStream; used when codegen holds &LinInterp_CdStream and reaches back 0x14 bytes.
typedef struct _CdAudioLocEx {
    /* 0x00 */ u8  field_0;
    /* 0x01 */ u8  field_1;
    /* 0x02 */ u16 field_2;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
    /* 0x10 */ s32 field_10;
} CdAudioLocEx;
STATIC_ASSERT_SIZEOF(CdAudioLocEx, 0x14);

/// Extended view of the CdAudio_Loc BSS block for SPU voice indices at +0x3E/+0x3F
/// (used by CdAudio_DrivePhase0 / CdAudio_DriveRead). The zero-init in CdAudio_Init covers
/// 0x44 bytes from CdAudio_Loc, so these offsets sit inside that block.
typedef struct _CdAudioVoices {
    /* 0x00 */ u8 pad[0x3E];
    /* 0x3E */ s8 field_3E;
    /* 0x3F */ s8 field_3F;
} CdAudioVoices;
STATIC_ASSERT_SIZEOF(CdAudioVoices, 0x40);

/// "oneA" (0x41656E6F) tagged chunk header read by func_8005664C.
/// Located at a signed byte offset into a raw buffer.
typedef struct _SndOneA {
    /* 0x0 */ s32 field_0;
    /* 0x4 */ u16 field_4;
    /* 0x6 */ u16 field_6;
} SndOneA;
STATIC_ASSERT_SIZEOF(SndOneA, 0x8);

/// Destination for func_8005664C: receives halfwords from a SndOneA chunk.
typedef struct _SndOneAOut {
    /* 0x00 */ u8  pad_00[0x3A];
    /* 0x3A */ u16 field_3A;
    /* 0x3C */ u16 field_3C;
} SndOneAOut;
STATIC_ASSERT_SIZEOF(SndOneAOut, 0x3E);

/// Dialog / prompt descriptor used by 21FDC.c handlers (e.g. McMenu_ConfirmDialogAlt,
/// McMenu_ConfirmDialog, McMenu_ConfirmWithRender). field_8 is a signed menu/option index passed
/// to rendering helpers; field_B is a flag written on the alternate confirm
/// path; field_C is a selection/confirm flag (1 = confirm); field_18/field_1A
/// are position halfwords; field_1C is data passed through to Text_DrawPrompt;
/// field_22 is a state halfword set with the alternate confirm path.
typedef struct _DialogPrompt {
    /* 0x00 */ byte unknown_0[0x8];
    /* 0x08 */ s8   field_8;
    /* 0x09 */ byte unknown_9[0x2];
    /* 0x0B */ s8   field_B;
    /* 0x0C */ s32  field_C;
    /* 0x10 */ byte unknown_10[0x8];
    /* 0x18 */ s16  field_18;
    /* 0x1A */ s16  field_1A;
    /* 0x1C */ s32  field_1C;
    /* 0x20 */ byte unknown_20[0x2];
    /* 0x22 */ s16  field_22;
} DialogPrompt;

/// Linked text option node walked by func_80049AF0 (index via DialogPrompt::field_8).
/// field_0 is the string passed to Text_DrawPrompt; field_4 is the next node.
typedef struct _DialogOption {
    /* 0x0 */ u8*                   field_0;
    /* 0x4 */ struct _DialogOption* field_4;
} DialogOption;
STATIC_ASSERT_SIZEOF(DialogOption, 0x8);

/// Context at Task::field_34 for the func_80049AF0 dialog path.
/// field_4 is the head of a DialogOption list; field_C bit0 gates cancel input.
typedef struct _DialogListCtx {
    /* 0x00 */ byte          unknown_0[4];
    /* 0x04 */ DialogOption* field_4;
    /* 0x08 */ byte          unknown_8[4];
    /* 0x0C */ u8            field_C;
} DialogListCtx;

/// Context at Task::field_34 for the func_80049C00 UI path.
/// field_0 is a base index copied into UiList field_4/field_5; field_2 receives
/// the selected index from UiObject::field_2C on confirm/cancel; field_8 is an
/// optional string passed to Ui_DrawText.
typedef struct _SelectMenuCtx {
    /* 0x00 */ u8    field_0;
    /* 0x01 */ byte  pad_1;
    /* 0x02 */ s16   field_2;
    /* 0x04 */ byte  pad_4[4];
    /* 0x08 */ char* field_8;
} SelectMenuCtx;

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

/// 0xC-byte init-table entry at Snd_BankInitTable (two entries used by func_80053FF4).
/// field_0 indexes D_800680AC for a slot id; field_2 is written to SndBankSlot.field_8
/// and SndBank.field_8; field_4/field_6 are F3D458_Malloc sizes; field_8 is stored
/// to SndBankSlot.field_C.
typedef struct _SndBankInitEntry {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
    /* 0x4 */ u16 field_4;
    /* 0x6 */ u16 field_6;
    /* 0x8 */ s32 field_8;
} SndBankInitEntry;
STATIC_ASSERT_SIZEOF(SndBankInitEntry, 0xC);

/// Pointer to the start of the game heap.
extern u8* GHeap;

/// Pointer to the auxilary heap.
extern u8* GAuxHeap;

/// Length in bytes of the heap pointed to by `GAuxHeap`.
extern size_t GAuxHeapSize;

extern size_t D_80068F88;

/// Pointer to the active auxilary heap.
extern u8* GActiveAuxHeap;

extern size_t D_80068F90;

/// Length in bytes of the heap pointed to by `GActiveAuxHeap`.
extern size_t GActiveAuxHeapSize;

extern int D_80068F98;
// CdCmd_Queue, pad at 0x80068F9C, D_800691F4/F8: see main.c / main/fs.h
extern u8*    D_800691F4;
extern size_t D_800691F8;

#endif
