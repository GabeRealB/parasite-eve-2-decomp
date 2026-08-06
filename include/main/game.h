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

// CdCmdEntry / CdCmdQueue live in main/fs.h

typedef struct _GStruct4 {
    u8 field_0;
    u8 field_1;
    u8 field_2;
    u8 field_3;
    u8 field_4;
    u8 field_5;
} GStruct4;
STATIC_ASSERT_SIZEOF(GStruct4, 0x6);

typedef struct _GStruct5 {
    byte unknown_0[0x4];
    s16  field_4;
    s16  field_6;
    byte unknown_8[0x18];
} GStruct5;
STATIC_ASSERT_SIZEOF(GStruct5, 0x20);

typedef struct _HeapBlockHeader {
    u32                      size;
    u16                      isAllocated;
    u16                      magic;
    struct _HeapBlockHeader* prev;
    struct _HeapBlockHeader* next;
} HeapBlockHeader;
STATIC_ASSERT_SIZEOF(HeapBlockHeader, 0x10);

typedef struct _GStruct7 {
    u32           enableVoices;
    u32           disableVoices;
    u32           reverbMode;
    u32           isDirty;
    SpuReverbAttr attr;
} GStruct7;
STATIC_ASSERT_SIZEOF(GStruct7, 0x24);

typedef struct _GStruct8 {
    s32 field_0;
    s32 field_4;
    s16 field_8;
    s32 field_c;
    s32 field_10;
    s32 field_14;
} GStruct8;
STATIC_ASSERT_SIZEOF(GStruct8, 0x18);

typedef struct _GStruct9 {
    u32 reverbVoiceStatus;
    u32 field_4[24];
    u8  field_64[24];
    u8  field_7c[24];
    u8  field_94[24];
    u32 field_ac[24];
    u32 field_10c[24];
    u32 field_16c[24];
    u32 field_1cc;
    u32 field_1d0;
} GStruct9;
STATIC_ASSERT_SIZEOF(GStruct9, 0x1D4);

typedef struct _GStruct10 {
    s16           count;
    SpuLVoiceAttr attrs[24];
    u8            field_664[24];
} GStruct10;
STATIC_ASSERT_SIZEOF(GStruct10, 0x67C);

// FsImgBuffers / FsWorkEntry / FsLoadParams live in main/fs.h

/// 4-byte block assigned via unaligned lwl/lwr (see func_800429C8).
typedef struct _GBytes4 {
    u8 data[4];
} GBytes4;

/// 6-byte block assigned via unaligned lwl/lwr + lb/sb (see func_80053BF4).
typedef struct _GBytes6 {
    u8 data[6];
} GBytes6;

/// 8-byte block assigned via unaligned lwl/lwr (see func_8002BF10).
typedef struct _GBytes8 {
    u8 data[8];
} GBytes8;

/// Overlay of objects with an 8-byte field at offset 0x4 (GStruct14, McSaveData).
typedef struct _GStructOverlayAt4 {
    byte    pad[4];
    GBytes8 field_4;
} GStructOverlayAt4;
STATIC_ASSERT_SIZEOF(GStructOverlayAt4, 0xC);

typedef struct _GStruct14 {
    byte  unknown_0[0x2];
    u8    field_2;
    byte  unknown_3;
    byte  field_4; // address taken by func_8001D39C
    byte  unknown_5;
    u8    field_6;
    u8    field_7;
    byte  unknown_8[0x4];
    void* field_C[16]; // 0xC..0x4B; cleared by func_8002D780
    byte  unknown_4C;
    u8    field_4D;
    byte  unknown_4E[0x26];
    u8    field_74;
    byte  unknown_75[0x7];
    s16   field_7C;
    byte  unknown_7E[0xBE];
} GStruct14;
STATIC_ASSERT_SIZEOF(GStruct14, 0x13C);

/// Overlay of `GStruct14` starting at offset 0x4 (`field_4`..`field_7`).
/// Used when the compiler keeps `&D4F564_8005ED64->field_4` in a register.
typedef struct _GStruct14From4 {
    /* 0x0 */ u8 field_0; // GStruct14.field_4
    /* 0x1 */ u8 field_1; // GStruct14.unknown_5
    /* 0x2 */ u8 field_2; // GStruct14.field_6
    /* 0x3 */ u8 field_3; // GStruct14.field_7
} GStruct14From4;
STATIC_ASSERT_SIZEOF(GStruct14From4, 0x4);

/// Large object pointed to by Task::field_1C for the slot-3 game object
/// (func_8002D22C(3)). Sparse fields used by func_8003EE68.
typedef struct _GStruct71 {
    /* 0x000 */ byte pad_0[0x90];
    /* 0x090 */ s32  field_90;
    /* 0x094 */ byte pad_94[0xE8];
    /* 0x17C */ byte field_17C; // address taken for func_800E1A6C
    /* 0x17D */ byte pad_17D[0x7B3];
    /* 0x930 */ byte field_930; // address taken for func_801011D0
    /* 0x931 */ byte pad_931[0x53];
    /* 0x984 */ u8   field_984;
} GStruct71;
STATIC_ASSERT_SIZEOF(GStruct71, 0x988);

/// Object pointed to by Task::field_2c; field_8 is a s32* cleared by
/// func_8003EE68 after optional func_801011D0 / func_800E1A6C setup.
typedef struct _GStruct72 {
    /* 0x0 */ byte pad_0[0x8];
    /* 0x8 */ s32* field_8;
} GStruct72;
STATIC_ASSERT_SIZEOF(GStruct72, 0xC);

/// 0x1C-byte slot allocated from D_8007EBF0 (see func_800509F4 / func_80050A38).
/// Overlay of `GStruct16` starting at offset 0x4 (`field_4` / `field_8`).
/// Used when the compiler keeps `arg + 4` in a callee-saved register.
typedef struct _GStruct16From4 {
    u8  field_0;
    u8  field_1;
    u16 field_2;
    s32 field_4;
    s32 field_8;
    s32 field_C;
} GStruct16From4;
STATIC_ASSERT_SIZEOF(GStruct16From4, 0x10);

typedef struct _GStruct16 {
    s16                field_0;
    s16                field_2;
    u8                 field_4;
    u8                 field_5;
    u16                field_6;
    s32                field_8;
    s32                field_C;
    s32                field_10;
    struct _GStruct16* field_14;
    struct _GStruct16* field_18;
} GStruct16;
STATIC_ASSERT_SIZEOF(GStruct16, 0x1C);

/// Pointed to by D_80062698 (bss object D_8007A320, size 0x38).
typedef struct _GStruct17 {
    /* 0x00 */ TaskDesc* field_0;
    /* 0x04 */ s32       field_4;
    /* 0x08 */ s32       field_8;
    /* 0x0C */ u32       field_C;
    /* 0x10 */ byte      unknown_10;
    /* 0x11 */ u8        field_11;
    /* 0x12 */ u8        field_12;
    /* 0x13 */ u8        field_13;
    /* 0x14 */ u8        field_14;
    /* 0x15 */ u8        field_15;
    /* 0x16 */ byte      unknown_16;
    /* 0x17 */ u8        field_17;
    /* 0x18 */ u8        field_18;
    /* 0x19 */ u8        field_19;
    /* 0x1A */ u8        field_1a;
    /* 0x1B */ byte      unknown_1b;
    /* 0x1C */ u32       field_1c;
    /* 0x20 */ s32       field_20;
    /* 0x24 */ s32       field_24;
    /* 0x28 */ s32       field_28;
    /* 0x2C */ u8        field_2C[8];
    /* 0x34 */ u8        field_34[4];
} GStruct17;
STATIC_ASSERT_SIZEOF(GStruct17, 0x38);

/// BSS object D_800827A0 (size 0x10).
typedef struct _GStruct18 {
    u8  field_0;
    u8  field_1;
    u16 field_2;
    s32 field_4;
    s32 field_8;
    s32 field_C;
} GStruct18;
STATIC_ASSERT_SIZEOF(GStruct18, 0x10);

/// BSS object D_80082818 (size 0x58).
typedef struct _GStruct19 {
    /* 0x00 */ byte unknown_0[0x6];
    /* 0x06 */ s16  field_6;
    /* 0x08 */ void (*field_8)(s32);
    /* 0x0C */ byte unknown_C[0x8];
    /* 0x14 */ s32  field_14;
    /* 0x18 */ s32  field_18;
    /* 0x1C */ s32  field_1C;
    /* 0x20 */ s32  field_20;
    /* 0x24 */ byte unknown_24[0x4];
    /* 0x28 */ s32  field_28;
    /* 0x2C */ byte unknown_2C[0x4];
    /* 0x30 */ s32  field_30;
    /* 0x34 */ byte unknown_34[0x8];
    /* 0x3C */ s32  field_3C;
    /* 0x40 */ s16  field_40;
    /* 0x42 */ s16  field_42;
    /* 0x44 */ byte unknown_44[0x8];
    /* 0x4C */ s16  field_4C;
    /* 0x4E */ byte unknown_4E[0x5];
    /* 0x53 */ u8   field_53;
    /* 0x54 */ u16  field_54;
    /* 0x56 */ byte unknown_56[0x2];
} GStruct19;
STATIC_ASSERT_SIZEOF(GStruct19, 0x58);

/// One half of D_80082870 (stride 0x40). Dual L/R audio channel state.
typedef struct _GStruct74Entry {
    /* 0x00 */ byte unknown_0[4];
    /* 0x04 */ s32  field_4;
    /* 0x08 */ s16  field_8;
    /* 0x0A */ s16  field_A;
    /* 0x0C */ byte unknown_C[0x34];
} GStruct74Entry;
STATIC_ASSERT_SIZEOF(GStruct74Entry, 0x40);

/// BSS object D_80082870 (size 0x80). Two GStruct74Entry channels at +0x00 / +0x40.
typedef struct _GStruct74 {
    /* 0x00 */ GStruct74Entry ch[2];
} GStruct74;
STATIC_ASSERT_SIZEOF(GStruct74, 0x80);

/// One slot in D_800828F0.entries (stride 0x14). field_0 holds status flags.
typedef struct _GStruct32Entry {
    /* 0x00 */ u32 field_0;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
    /* 0x10 */ s32 field_10;
} GStruct32Entry;
STATIC_ASSERT_SIZEOF(GStruct32Entry, 0x14);

/// BSS object D_800828F0 (size 0x58). CD ready-callback install state.
typedef struct _GStruct32 {
    /* 0x00 */ u8             field_0;
    /* 0x01 */ u8             field_1;
    /* 0x02 */ u8             field_2;
    /* 0x03 */ u8             field_3;
    /* 0x04 */ void*          field_4;
    /* 0x08 */ GStruct32Entry entries[4];
} GStruct32;
STATIC_ASSERT_SIZEOF(GStruct32, 0x58);

/// Object passed to func_80048C10 / func_80048D58 (e.g. via Task::field_20).
typedef struct _GStruct20 {
    /* 0x00 */ s32  field_0;
    /* 0x04 */ byte unknown_4[0x18];
    /* 0x1C */ s16  field_1c;
    /* 0x1E */ s16  unknown_1e;
    /* 0x20 */ s16  field_20;
    /* 0x22 */ s16  field_22;
} GStruct20;
STATIC_ASSERT_SIZEOF(GStruct20, 0x24);

/// Object at Task::field_20 used by func_80048838 / Mc_HideChildUi /
/// Mc_DrawPrompt / func_800486F0. Shares the GStruct30 layout through offset
/// 0x24 (handlers cast field_20 to GStruct30*). field_0 is a status flag;
/// field_4 is copied from UiObjectDesc::field_0 at spawn; field_8 is a mode
/// (5 = skip draw in func_8002FDCC / func_8002FB84; set to 3 when torn down); field_C..field_12
/// are layout halfwords (RECT-like); field_14 is a halfword counter used as the
/// text draw priority/order; field_16 is a signed timer/counter; field_18/field_1A
/// are layout offsets (shared with GStruct30; used when positioning child UI);
/// field_1C is a position halfword (+2 when passed to func_8002FDCC); field_1E is
/// an x offset paired with field_20; field_20/field_22 are base x/y for relative
/// text placement; field_24 is a callback copied from the descriptor; field_28 is
/// the owning Task*; field_2C / field_2E are halfwords polled by teardown state
/// handlers (e.g. func_8002BD24 waits until field_2E == -1 before cleaning up;
/// dialog pickers set field_2E == 6 when a choice is confirmed).
typedef struct _UiObject {
    /* 0x00 */ s32   field_0;
    /* 0x04 */ s32   field_4;
    /* 0x08 */ s32   field_8;
    /* 0x0C */ u16   field_C;
    /* 0x0E */ u16   field_E;
    /* 0x10 */ u16   field_10;
    /* 0x12 */ u16   field_12;
    /* 0x14 */ u16   field_14;
    /* 0x16 */ s16   field_16;
    /* 0x18 */ u16   field_18;
    /* 0x1A */ u16   field_1A;
    /* 0x1C */ s16   field_1C;
    /* 0x1E */ u16   field_1E;
    /* 0x20 */ u16   field_20;
    /* 0x22 */ u16   field_22;
    /* 0x24 */ s32   field_24;
    /* 0x28 */ Task* field_28;
    /* 0x2C */ s16   field_2C;
    /* 0x2E */ s16   field_2E;
} UiObject;
STATIC_ASSERT_SIZEOF(UiObject, 0x30);

/// Template/descriptor consumed by func_800486F0 to spawn a UiObject + Task.
/// field_10/field_12/field_18 seed a stack TaskDesc; field_0 and field_4..field_C
/// / field_14 are copied onto the allocated UiObject.
typedef struct _UiObjectDesc {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ u16 field_4;
    /* 0x06 */ u16 field_6;
    /* 0x08 */ u16 field_8;
    /* 0x0A */ u16 field_A;
    /* 0x0C */ u16 field_C;
    /* 0x0E */ u16 field_E;
    /* 0x10 */ u16 field_10;
    /* 0x12 */ u16 field_12;
    /* 0x14 */ s32 field_14;
    /* 0x18 */ s32 field_18;
} UiObjectDesc;
STATIC_ASSERT_SIZEOF(UiObjectDesc, 0x1C);

/// 8-byte slot at GStruct22::field_484 (16 entries, indexed by opcode low nibble).
/// Seeded as the word 0x407F4000 by func_800528BC (field_0..field_3 little-endian);
/// field_0 is tested as a flag byte (lbu); field_4 is a byte written by handlers.
typedef struct _GStruct22Entry {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ u8  field_2;
    /* 0x3 */ u8  field_3;
    /* 0x4 */ u8  field_4;
    /* 0x5 */ u8  field_5;
    /* 0x6 */ s16 field_6;
} GStruct22Entry;
STATIC_ASSERT_SIZEOF(GStruct22Entry, 0x8);

/// Large context object used by 410B0.c opcode handlers (e.g. func_800529BC).
/// Only fields used so far are named; size is incomplete.
typedef struct _GStruct22 {
    /* 0x000 */ byte           unknown_0[0x484];
    /* 0x484 */ GStruct22Entry field_484[16];
} GStruct22;

/// Element of BSS array D_8006D4F0 (15 entries, total 0x258).
/// Fields inferred from F344.c accessors (func_8001ED20, func_8001EED8, etc.).
typedef struct _GStruct24 {
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
} GStruct24;
STATIC_ASSERT_SIZEOF(GStruct24, 0x28);

/// UI list/menu object (data symbols D_8006116C, D_80061194, D_8006125C,
/// D_80061284, D_800612AC, D_80067654; size 0x24).
/// field_0 is a function-table pointer; field_4 / field_5 are base indices
/// (func_80049C00 seeds both from context); field_5 is also subtracted when
/// computing field_9; field_6 / field_7 are signed layout sizes (func_80046DEC
/// uses field_7 as TILE height); field_9 / field_A / field_10 are list cursor /
/// flag / selection index used by func_80036A70 / func_80036C04 / func_80037068 /
/// func_800489A0 / func_8004917C; field_C / field_14 / field_16 are cleared by
/// func_800489A0; field_17 is a signed layout adjust subtracted from the child
/// height when computing visible rows (func_80048AEC / func_80048C30; the latter
/// also writes field_17 from its third argument).
typedef struct _UiList {
    /* 0x00 */ byte unknown_0[0x4];
    /* 0x04 */ u8   field_4;
    /* 0x05 */ u8   field_5;
    /* 0x06 */ s8   field_6;
    /* 0x07 */ s8   field_7;
    /* 0x08 */ byte unknown_8;
    /* 0x09 */ u8   field_9;
    /* 0x0A */ u8   field_A;
    /* 0x0B */ byte unknown_B;
    /* 0x0C */ s32  field_C;
    /* 0x10 */ s32  field_10;
    /* 0x14 */ s16  field_14;
    /* 0x16 */ s8   field_16;
    /* 0x17 */ s8   field_17;
    /* 0x18 */ byte unknown_18[0xC];
} UiList;
STATIC_ASSERT_SIZEOF(UiList, 0x24);

/// Context pointed to by Task::field_34 in the D_8006121C select-menu path
/// (func_80036C04). Only field_290 is named so far.
typedef struct _GStruct64 {
    /* 0x000 */ byte unknown_0[0x290];
    /* 0x290 */ s32  field_290;
} GStruct64;

/// 4-byte entry at D_8007EB98 (see func_8004E5A0).
typedef struct _GStruct26 {
    s16 field_0;
    s16 field_2;
} GStruct26;
STATIC_ASSERT_SIZEOF(GStruct26, 0x4);

/// Source/model data pointed to by GStruct27::field_10 (see func_80041700).
/// field_0 is a one-shot init flag (func_800409D0 sets it to 1).
/// field_4 is used as a byte-count for aux-heap allocations (calloc size * 2).
/// field_20 points at a stream of [id, handler_slot, dims, data...] words.
typedef struct _GStruct33 {
    /* 0x00 */ s32  field_0;
    /* 0x04 */ s32  field_4;
    /* 0x08 */ byte unknown_8[0x18];
    /* 0x20 */ u32* field_20;
} GStruct33;

/// Node in the D_800711B8 linked list (2F244.c TMD/model objects).
/// Header is 0x34 bytes with a variable payload after. Fields from func_80041700
/// init and related free/alloc helpers (func_80041B4C, func_80041B88, etc.).
typedef struct _GStruct27 {
    /* 0x00 */ struct _GStruct27* next;
    /* 0x04 */ byte               unknown_4[0x4];
    /* 0x08 */ void*              field_8;
    /* 0x0C */ u16                field_C;
    /* 0x0E */ s8                 field_E;
    /* 0x0F */ byte               unknown_F;
    /* 0x10 */ GStruct33*         field_10;
    /* 0x14 */ u16                field_14;
    /* 0x16 */ u16                field_16;
    /* 0x18 */ void*              field_18;
    /* 0x1C */ void*              field_1C;
    /* 0x20 */ void*              field_20;
    /* 0x24 */ u8                 field_24;
    /* 0x25 */ u8                 field_25;
    /* 0x26 */ byte               unknown_26[0xA];
    /* 0x30 */ s32                field_30;
} GStruct27;
STATIC_ASSERT_SIZEOF(GStruct27, 0x34);

/// Sentinel list head for GStruct27 (and similar) intrusive lists.
/// Same layout as TaskNode: next is the first element, prev is the last
/// (or &self when the list is empty). Initialized by func_80028718.
typedef struct _GStruct27Head {
    /* 0x00 */ GStruct27*             next;
    /* 0x04 */ struct _GStruct27Head* prev;
} GStruct27Head;
STATIC_ASSERT_SIZEOF(GStruct27Head, 0x8);

/// Head of the GStruct27 linked list used by 2F244.c TMD/model helpers.
extern GStruct27Head D_800711B8;
/// Second list head initialized alongside D_800711B8 by func_80028718.
extern GStruct27Head D_800711C0;

/// Nested object reached via GStruct29::field_28 (see func_80049D34).
/// Only field_34 is named so far.
typedef struct _GStruct28 {
    /* 0x00 */ byte unknown_0[0x34];
    /* 0x34 */ s32  field_34;
} GStruct28;

/// Object pointed to by D_80067694. field_28 is a child pointer written by
/// UI/context setup; func_80049D34 stores through it.
typedef struct _GStruct29 {
    /* 0x00 */ byte       unknown_0[0x28];
    /* 0x28 */ GStruct28* field_28;
} GStruct29;

/// Object used by 34E98.c handlers (e.g. func_80049554 / D_80013F2C table).
/// field_4 low nibble selects layout padding (func_80049348); high nibble of the
/// low byte selects a fill mode (func_80045A3C). field_8 is a small integer
/// state; field_C is a source RECT used by layout helpers (func_80049348 /
/// func_80049478); field_14 is a halfword counter temporarily adjusted around
/// text draw (func_80048F88); field_16 is a signed counter/timer;
/// field_18..field_22 are layout offsets (func_80049024 / func_80049348);
/// field_24 is a callback invoked with the second handler argument.
typedef struct _GStruct30 {
    /* 0x00 */ s32  field_0;
    /* 0x04 */ s32  field_4;
    /* 0x08 */ s32  field_8;
    /* 0x0C */ RECT field_C;
    /* 0x14 */ u16  field_14;
    /* 0x16 */ s16  field_16;
    /* 0x18 */ u16  field_18;
    /* 0x1A */ u16  field_1A;
    /* 0x1C */ u16  field_1C;
    /* 0x1E */ u16  field_1E;
    /* 0x20 */ u16  field_20;
    /* 0x22 */ u16  field_22;
    /* 0x24 */ void (*field_24)(void*);
} GStruct30;

/// Callback for GStruct30 state handlers (e.g. entries in D_80013F2C).
typedef void (*GFunc30)(GStruct30* arg0, void* arg1);

/// Fixed-size table of GFunc30 callbacks. Copied onto the stack by
/// func_800498D4 so the call uses a local jump table.
typedef struct {
    GFunc30 funcs[6];
} GFunc30Table6;

/// Header for the bank table blob pointed to by GStruct31.field_0.
/// field_4 is the bank ID (high halfword remapped by func_80053F00 when the
/// request high nibble is 0x1); field_6 is the entry count used by func_8005414C.
/// A u16 offset table follows at 0x8 (indexed via GStruct45OffsetView).
typedef struct _GStruct45 {
    /* 0x0 */ u8  unknown_0[4];
    /* 0x4 */ u16 field_4;
    /* 0x6 */ u16 field_6;
} GStruct45;
STATIC_ASSERT_SIZEOF(GStruct45, 0x8);

/// Overlay for reading the u16 offset table that follows GStruct45 at +0x8.
/// Formed as (GStruct45OffsetView*)((index * 2) + (s32)header) so lhu 8(base)
/// picks offsets[index] (func_8005414C).
typedef struct _GStruct45OffsetView {
    /* 0x0 */ u8  pad[8];
    /* 0x8 */ u16 field_8;
} GStruct45OffsetView;
STATIC_ASSERT_SIZEOF(GStruct45OffsetView, 0xA);

/// 16-byte slot in D_80082148[16] (BSS size 0x100). Indexed by func_800561C0
/// and related helpers in 43FFC.c / 410B0.c.
typedef struct _GStruct31 {
    /* 0x0 */ GStruct45* field_0;
    /* 0x4 */ void*      field_4;
    /* 0x8 */ s32        field_8;
    /* 0xC */ void*      field_C;
} GStruct31;
STATIC_ASSERT_SIZEOF(GStruct31, 0x10);

/// Owner of a doubly-linked GStruct43 voice list (head at field_40).
/// Insert: func_800562B4; unlink: func_80056068; walk: func_800563B4.
/// field_44 is a pointer to the raw oneE/script buffer base (func_800565B8).
typedef struct _GStruct43 GStruct43;
typedef struct _GStruct57 {
    /* 0x00 */ u8         unknown_0[0x40];
    /* 0x40 */ GStruct43* field_40;
    /* 0x44 */ u8**       field_44;
} GStruct57;
STATIC_ASSERT_SIZEOF(GStruct57, 0x48);

/// FX/envelope sub-block embedded at GStruct43 + 0x10 (func_800565B8 / func_80055678).
/// field_0 is an active flag; field_1 is the state-machine index; field_2 is a
/// secondary gate; field_20 points at the current "oneE" (0x45656E6F) chunk.
typedef struct _GStruct43Fx {
    /* 0x00 */ s8   field_0;
    /* 0x01 */ s8   field_1;
    /* 0x02 */ s8   field_2;
    /* 0x03 */ u8   pad_3;
    /* 0x04 */ s32  field_4;
    /* 0x08 */ s16  field_8;
    /* 0x0A */ s16  field_A;
    /* 0x0C */ s16  field_C;
    /* 0x0E */ u8   pad_E[0x6];
    /* 0x14 */ s32  field_14;
    /* 0x18 */ s32  field_18;
    /* 0x1C */ s32  field_1C;
    /* 0x20 */ s32* field_20;
} GStruct43Fx;
STATIC_ASSERT_SIZEOF(GStruct43Fx, 0x24);

/// Voice/FX object carved from D_80082148 with stride 0x40 (func_80056240).
/// field_0 is the SPU voice index; field_4 is a countdown/timer (func_80056308).
/// field_10/field_12 gate FX processing (aliases of fx.field_0 / fx.field_2).
/// field_34/field_38/field_3C are parent/prev/next list links (func_80056068 free).
struct _GStruct43 {
    /* 0x00 */ s8         field_0;
    /* 0x01 */ u8         field_1;
    /* 0x02 */ s8         field_2;
    /* 0x03 */ u8         field_3;
    /* 0x04 */ s32        field_4;
    /* 0x08 */ s16        field_8;
    /* 0x0A */ u8         field_A;
    /* 0x0B */ u8         unknown_0B[0x5];
    /* 0x10 */ s8         field_10;
    /* 0x11 */ s8         field_11;
    /* 0x12 */ s8         field_12;
    /* 0x13 */ u8         unknown_13;
    /* 0x14 */ s32        field_14;
    /* 0x18 */ s16        field_18;
    /* 0x1A */ s16        field_1A;
    /* 0x1C */ s16        field_1C;
    /* 0x1E */ u8         unknown_1E[0x6];
    /* 0x24 */ s32        field_24;
    /* 0x28 */ s32        field_28;
    /* 0x2C */ s32        field_2C;
    /* 0x30 */ s32*       field_30;
    /* 0x34 */ GStruct57* field_34;
    /* 0x38 */ GStruct43* field_38;
    /* 0x3C */ GStruct43* field_3C;
};
STATIC_ASSERT_SIZEOF(GStruct43, 0x40);

/// Overlay of GStruct34 at +0x1C (sector payload header for sound-bank setup).
/// Passed to func_8004CE28; filled from a CD sector by func_80052B30.
/// field_4 high nibble indexes D_800680AC / selects bank type; field_7 is the
/// GStruct42Group count and field_8 is the GStruct41 entry count used to size
/// the GStruct42 heap block (groups*4 + entries*0x14 + groups*2).
typedef struct _GStruct34Payload {
    /* 0x0 */ u8  pad_0[4];
    /* 0x4 */ u16 field_4;
    /* 0x6 */ u8  field_6;
    /* 0x7 */ u8  field_7;
    /* 0x8 */ u8  field_8;
} GStruct34Payload;

/// State block at D_800820F0; field_3 is also D_800820F3.
/// field_14/field_18 cleared by func_800537FC; field_10 sized by func_8005363C.
/// field_26/field_28 set by the CD ready path in func_800572FC.
/// Bytes at 0x1C.. are filled from a sector payload by func_80052B30.
/// Named BSS symbols D_80082120+ begin immediately after this 0x30-byte block.
typedef struct _GStruct34 {
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
    /* 0x1C */ byte  unknown_1C[0x8];
    /* 0x24 */ u8    field_24;
    /* 0x25 */ byte  unknown_25;
    /* 0x26 */ s16   field_26;
    /* 0x28 */ u8    field_28;
    /* 0x29 */ byte  unknown_29;
    /* 0x2A */ u16   field_2A;
    /* 0x2C */ s32   field_2C;
} GStruct34;
STATIC_ASSERT_SIZEOF(GStruct34, 0x30);

/// Per-buffer OT context (D_80070EE8[2]). Indexed by display buffer (stride 0x14).
/// field_4 is OT start; field_10 is the last tag (passed to DrawOTag).
typedef struct _GStruct35 {
    /* 0x00 */ s32     field_0;
    /* 0x04 */ u_long* field_4;
    /* 0x08 */ u8      unknown_08[0x8];
    /* 0x10 */ u_long* field_10;
} GStruct35;
STATIC_ASSERT_SIZEOF(GStruct35, 0x14);

/// Track/channel entry inside GStruct36 (stride 0x3C). field_5 is a per-entry flag
/// written by func_80051AB8; absolute offset of first entry's field_5 is 0x51.
/// field_0 / field_1 / field_4 are NRPN/RPN state used by the MIDI CC handler
/// (func_80052488). field_6 / field_7 and field_8[] form a loop stack for the
/// 0xF5/0xF6 meta opcodes (func_800526A4); field_8[8] is also the track data
/// pointer resolved by func_80051A2C (absolute offset 0x74). field_30 is a
/// saved event cursor for looped CC 0x63.
typedef struct _GStruct36Entry {
    /* 0x00 */ u8  field_0;
    /* 0x01 */ u8  field_1;
    /* 0x02 */ u8  field_2;
    /* 0x03 */ u8  field_3;
    /* 0x04 */ u8  field_4;
    /* 0x05 */ u8  field_5;
    /* 0x06 */ u8  field_6;
    /* 0x07 */ s8  field_7;
    /* 0x08 */ u8* field_8[9];
    /* 0x2C */ u8  unknown_2C[4];
    /* 0x30 */ u8* field_30;
    /* 0x34 */ u8  unknown_34[8];
} GStruct36Entry;
STATIC_ASSERT_SIZEOF(GStruct36Entry, 0x3C);

/// Active SPU voice slot inside GStruct36 (stride 0xC, 18 slots at 0x504).
/// field_0 is the voice index (negative when free); iterated by func_80051AF0.
/// field_0 / field_1 are set to -1 when the slot is cleared (func_80051964).
/// field_1 / field_2 match opcode nibble / param in func_800528F8.
/// field_6 / field_7 index the bank note via func_8004EA60; field_8 is scaled pitch.
typedef struct _GStruct36VoiceSlot {
    /* 0x0 */ s8  field_0;
    /* 0x1 */ s8  field_1;
    /* 0x2 */ s8  field_2;
    /* 0x3 */ u8  unknown_3[0x3];
    /* 0x6 */ u8  field_6;
    /* 0x7 */ u8  field_7;
    /* 0x8 */ s16 field_8;
    /* 0xA */ u8  unknown_A[0x2];
} GStruct36VoiceSlot;
STATIC_ASSERT_SIZEOF(GStruct36VoiceSlot, 0xC);

/// 0x10-byte linear interpolator state used by func_8004D200 / func_8004D298 /
/// func_8004D2EC. Embedded at GStruct36::field_14; BSS object D_800827B4 sits
/// 0x14 bytes after D_800827A0.
typedef struct _GStruct55 {
    /* 0x0 */ s32 field_0;
    /* 0x4 */ s32 field_4;
    /* 0x8 */ s32 field_8;
    /* 0xC */ s16 field_C;
    /* 0xE */ s16 field_E;
} GStruct55;
STATIC_ASSERT_SIZEOF(GStruct55, 0x10);

/// State block at D_8007F300 (logical stride 0x5DC; BSS allocation 0x5E0).
/// field_0 is status; field_3 is the number of track entries starting at 0x4C.
/// field_4/field_5 are copied from field_6/field_7 by the per-frame driver.
/// field_8 is a scaled volume; field_C is a sentinel (0xFFFF when cleared).
/// field_10 is a data pointer; field_14 is the volume interpolator.
/// field_484 is a 16-entry opcode table (same layout as GStruct22::field_484);
/// func_800528BC seeds each entry with 0x407F4000 / 0.
/// voiceSlots holds up to 18 active SPU voice indices (field_0 = -1 when free).
typedef struct _GStruct42 GStruct42;

typedef struct _GStruct36 {
    /* 0x00 */ u8                  field_0;
    /* 0x01 */ u8                  field_1;
    /* 0x02 */ u8                  field_2;
    /* 0x03 */ u8                  field_3;
    /* 0x04 */ u8                  field_4;
    /* 0x05 */ u8                  field_5;
    /* 0x06 */ u8                  field_6;
    /* 0x07 */ u8                  field_7;
    /* 0x08 */ s16                 field_8;
    /* 0x0A */ s16                 field_A;
    /* 0x0C */ s32                 field_C;
    /* 0x10 */ void*               field_10;
    /* 0x14 */ GStruct55           field_14;
    /* 0x24 */ u8                  unknown_24[0x18];
    /* 0x3C */ s32                 field_3C;
    /* 0x40 */ GStruct42*          field_40;
    /* 0x44 */ u8                  unknown_44[0x8];
    /* 0x4C */ GStruct36Entry      entries[1];
    /* 0x88 */ u8                  unknown_88[0x484 - 0x88];
    /* 0x484 */ GStruct22Entry     field_484[16];
    /* 0x504 */ GStruct36VoiceSlot voiceSlots[0x12];
} GStruct36;
STATIC_ASSERT_SIZEOF(GStruct36, 0x5DC);

/// Text-measure / draw-request block passed to func_8002EDFC / func_8002E53C.
/// field_0 / field_2 are position (or accumulate measured width); field_C selects
/// the glyph table; field_D selects centering mode (1 = half width, 2 = full);
/// field_F is added to glyph v when drawing sprites.
typedef struct _GStruct38 {
    /* 0x00 */ s16 field_0;
    /* 0x02 */ s16 field_2;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s8  field_C;
    /* 0x0D */ s8  field_D;
    /* 0x0E */ s8  field_E;
    /* 0x0F */ u8  field_F;
} GStruct38;
STATIC_ASSERT_SIZEOF(GStruct38, 0x10);

/// Per-glyph metrics in the font tables (D_8005EFB0 / D_8005FA30 / D_800604B0).
/// off_x / off_y are stored as bytes but used as signed offsets when drawing.
typedef struct _GStruct68 {
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
} GStruct68;
STATIC_ASSERT_SIZEOF(GStruct68, 0xC);

/// Draw params for func_800435F8 (SPRT) / func_80043854 (TILE).
/// field_0/field_2 are x/y; field_4/field_6 are texture u/v (SPRT only);
/// field_8/field_A are width/height (inclusive size, decremented when written);
/// field_C/D/E are RGB; field_10 selects shade-tex (0) vs semi-trans (nonzero).
typedef struct _GStruct65 {
    /* 0x00 */ s16 field_0;
    /* 0x02 */ s16 field_2;
    /* 0x04 */ u8  field_4;
    /* 0x05 */ u8  pad_5;
    /* 0x06 */ u8  field_6;
    /* 0x07 */ u8  pad_7;
    /* 0x08 */ s16 field_8;
    /* 0x0A */ s16 field_A;
    /* 0x0C */ u8  field_C;
    /* 0x0D */ u8  field_D;
    /* 0x0E */ u8  field_E;
    /* 0x0F */ u8  pad_F;
    /* 0x10 */ s16 field_10;
} GStruct65;
STATIC_ASSERT_SIZEOF(GStruct65, 0x12);

/// BSS object D_80082758 (size 0x18). CD/audio stream state for 46FE4.c.
/// field_C is a base pointer into a halfword table; func_80057A1C indexes it
/// with ((packed >> 14) & 0x3FC) / 2 (4-byte stride, low halfword of each slot).
typedef struct _GStruct39 {
    /* 0x00 */ u8   field_0;
    /* 0x01 */ u8   field_1;
    /* 0x02 */ u8   field_2;
    /* 0x03 */ u8   pad_3;
    /* 0x04 */ s32  field_4;
    /* 0x08 */ s32  field_8;
    /* 0x0C */ u16* field_C;
    /* 0x10 */ s32  field_10;
    /* 0x14 */ s32  field_14;
} GStruct39;
STATIC_ASSERT_SIZEOF(GStruct39, 0x18);

/// BSS object D_80073B88 (size 0x80). Initialized by func_8004C4D0.
/// field_18..field_1e are four s16 values set to 100; field_21/field_26 are flags.
/// field_40 is the upper half filled with 0xFF by func_800301FC.
typedef struct _GStruct40 {
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
} GStruct40;
STATIC_ASSERT_SIZEOF(GStruct40, 0x80);

/// 0x14-byte sound/note entry indexed by func_8004EA60.
/// Callers read field_1/field_3/field_4/field_5/field_A/field_B/field_10.
typedef struct _GStruct41 {
    /* 0x00 */ u8   field_0;
    /* 0x01 */ u8   field_1;
    /* 0x02 */ u8   pad_2;
    /* 0x03 */ u8   field_3;
    /* 0x04 */ u8   field_4;
    /* 0x05 */ u8   field_5;
    /* 0x06 */ byte unknown_6[0x4];
    /* 0x0A */ u8   field_A;
    /* 0x0B */ u8   field_B;
    /* 0x0C */ byte unknown_C[0x4];
    /* 0x10 */ s32  field_10;
} GStruct41;
STATIC_ASSERT_SIZEOF(GStruct41, 0x14);

/// 4-byte group header at the start of a GStruct42 heap block (field_0).
/// field_0 is the group size; func_8004D19C prefix-sums these into field_10.
typedef struct _GStruct42Group {
    /* 0x0 */ u8 field_0;
    /* 0x1 */ u8 pad[3];
} GStruct42Group;
STATIC_ASSERT_SIZEOF(GStruct42Group, 0x4);

/// Sound bank header used by func_8004EA60 (and D_8007E0D8 entries, stride 0x20).
/// field_0 points at GStruct42Group entries; field_4 is the base of GStruct41
/// entries; field_10 is a u16 prefix-sum index table built by func_8004D19C.
/// field_B is the group count. field_8 high nibble 0xF marks the slot free/invalid
/// (see func_8004D0A0 / func_8004D0F0). field_1C is a heap allocation released by
/// func_8004D0F0.
struct _GStruct42 {
    /* 0x00 */ GStruct42Group* field_0;
    /* 0x04 */ GStruct41*      field_4;
    /* 0x08 */ u16             field_8;
    /* 0x0A */ u8              field_A;
    /* 0x0B */ u8              field_B;
    /* 0x0C */ byte            unknown_C[0x4];
    /* 0x10 */ u16*            field_10;
    /* 0x14 */ void*           field_14;
    /* 0x18 */ s32             field_18;
    /* 0x1C */ void*           field_1C;
};
STATIC_ASSERT_SIZEOF(GStruct42, 0x20);

/// BSS object D_80082780 (size 0x14). CD stream control for 46FE4.c.
typedef struct _GStruct44 {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ u8  field_8;
    /* 0x09 */ u8  field_9;
    /* 0x0A */ s8  field_A;
    /* 0x0B */ u8  field_B;
    /* 0x0C */ s32 field_C;
    /* 0x10 */ s32 field_10;
} GStruct44;
STATIC_ASSERT_SIZEOF(GStruct44, 0x14);

/// BSS object D_800827C4 (size 0x20). CD/SPU stream setup block for
/// func_800567E4 / func_80057FAC: sector position, buffer, callbacks, and
/// voice indices.
typedef struct _GStruct76 {
    /* 0x00 */ s32   field_0;
    /* 0x04 */ s32   field_4;
    /* 0x08 */ void* field_8;
    /* 0x0C */ void  (*field_C)(void);
    /* 0x10 */ s32   field_10;
    /* 0x14 */ s32   field_14;
    /* 0x18 */ s16   field_18;
    /* 0x1A */ s8    field_1A;
    /* 0x1B */ s8    field_1B;
    /* 0x1C */ u8    field_1C;
    /* 0x1D */ u8    pad_1D[3];
} GStruct76;
STATIC_ASSERT_SIZEOF(GStruct76, 0x20);

/// Buffer with a 16-bit sum / ones-complement pair at the head and a payload
/// starting at offset 4. Written by `Mc_WriteBlockChecksum`, verified by `Mc_VerifyBlockChecksum`.
/// Out-parameter for `func_8004E5C4` (voice slot lookup/alloc).
/// field_0 is the voice index; field_4 points at the voice's SpuVoiceAttr.
typedef struct _GStruct48 {
    /* 0x0 */ s8            field_0;
    /* 0x1 */ s8            field_1;
    /* 0x2 */ s8            field_2;
    /* 0x3 */ s8            field_3;
    /* 0x4 */ SpuVoiceAttr* field_4;
} GStruct48;
STATIC_ASSERT_SIZEOF(GStruct48, 0x8);

/// Double-buffered ordering-table descriptor (same layout as PsyQ GsOT).
/// Used by D_8007A0E8 and passed to GsClearOt.
typedef struct _GStruct50 {
    /* 0x00 */ u_long  length;
    /* 0x04 */ u_long* org;
    /* 0x08 */ u_long  offset;
    /* 0x0C */ u_long  point;
    /* 0x10 */ u_long* tag;
} GStruct50;
STATIC_ASSERT_SIZEOF(GStruct50, 0x14);

/// PsyQ GsClearOt, declared with GStruct50* so callers need not include libgs.h.
void GsClearOt(unsigned short offset, unsigned short point, GStruct50* otp);

/// Callback-queue slot used by D_8007E2E0.entries / D_8007E2E4 (stride 0x14).
/// field_0 holds status flags (bit0 active, bit1 arm, bit2 pending, bit3 result).
typedef struct _GStruct51 {
    /* 0x00 */ s32  field_0;
    /* 0x04 */ s32  field_4;
    /* 0x08 */ s32  (*field_8)(struct _GStruct51*);
    /* 0x0C */ void (*field_C)(struct _GStruct51*);
    /* 0x10 */ s32  (*field_10)(struct _GStruct51*);
} GStruct51;
STATIC_ASSERT_SIZEOF(GStruct51, 0x14);

/// Ring buffer of 4 GStruct51 callback slots (D_8007E2E0, size 0x54).
/// field_0 is the read index; field_1 is the write index. D_8007E2E4 aliases entries.
typedef struct _GStruct51Queue {
    /* 0x00 */ s8        field_0;
    /* 0x01 */ s8        field_1;
    /* 0x02 */ u8        pad_2[2];
    /* 0x04 */ GStruct51 entries[4];
} GStruct51Queue;
STATIC_ASSERT_SIZEOF(GStruct51Queue, 0x54);

/// 4-byte entry pointed to by D_80082794 (see func_80057724).
/// Indexed by D_80082758.field_2; field_3 is compared across adjacent entries.
typedef struct _GStruct52 {
    /* 0x0 */ u8 pad[3];
    /* 0x3 */ u8 field_3;
} GStruct52;
STATIC_ASSERT_SIZEOF(GStruct52, 0x4);

/// Descriptor pointed to by GStruct16From4::field_C and passed to func_800558E8.
/// field_5 is a volume scale (0-127) used by func_80055DFC / func_80055078;
/// field_6 is a pitch bias; field_7 is a candidate-count threshold; field_8 is a
/// preference key for func_80055EF8; field_C/field_E are halfword IDs matched by
/// func_80054D58. Also the type of GStruct54::field_4C voice-param blocks
/// (field_E bit1 gates the D_80082749 volume override).
typedef struct _GStruct67 {
    /* 0x00 */ u8  pad_0[5];
    /* 0x05 */ u8  field_5;
    /* 0x06 */ u8  field_6;
    /* 0x07 */ u8  field_7;
    /* 0x08 */ s16 field_8;
    /* 0x0A */ u8  pad_A[2];
    /* 0x0C */ u16 field_C;
    /* 0x0E */ u16 field_E;
} GStruct67;
STATIC_ASSERT_SIZEOF(GStruct67, 0x10);

/// 0x60-byte slot in D_80082248[8]. field_0 is an ID looked up by
/// func_80055DAC; field_16 holds status flags (mask 0xA3 selects active entries).
/// field_E is a dirty flag; field_10/11/12 and field_13/14/15 are paired ramps
/// (current/target/step) updated by func_80055A9C and func_80055B70 respectively.
/// field_40 is the head of a GStruct43 voice list (cleared/walked by func_80055F70);
/// field_44/field_48 hold init params; field_F is bit1 of GStruct67::field_E.
/// field_4C is a voice-param block (volume scale at field_5) walked with field_40.
/// field_50 is a volume interpolator driven by func_800559BC via func_8004D200.
typedef struct _GStruct54 {
    /* 0x00 */ s32        field_0;
    /* 0x04 */ s32        field_4;
    /* 0x08 */ u8         pad_8[4];
    /* 0x0C */ s8         field_C;
    /* 0x0D */ s8         field_D;
    /* 0x0E */ s8         field_E;
    /* 0x0F */ s8         field_F;
    /* 0x10 */ u8         field_10;
    /* 0x11 */ u8         field_11;
    /* 0x12 */ s8         field_12;
    /* 0x13 */ u8         field_13;
    /* 0x14 */ u8         field_14;
    /* 0x15 */ s8         field_15;
    /* 0x16 */ u8         field_16;
    /* 0x17 */ u8         field_17;
    /* 0x18 */ u8         pad_18[0x28];
    /* 0x40 */ GStruct43* field_40;
    /* 0x44 */ s32        field_44;
    /* 0x48 */ GStruct67* field_48;
    /* 0x4C */ GStruct67* field_4C;
    /* 0x50 */ GStruct55  field_50;
} GStruct54;
STATIC_ASSERT_SIZEOF(GStruct54, 0x60);

/// BSS block covering D_800827A0 (0x10) + D_800827B0 (0x4). Immediately precedes
/// D_800827B4; used when codegen holds &D_800827B4 and reaches back 0x14 bytes.
typedef struct _GStruct56 {
    /* 0x00 */ u8  field_0;
    /* 0x01 */ u8  field_1;
    /* 0x02 */ u16 field_2;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
    /* 0x10 */ s32 field_10;
} GStruct56;
STATIC_ASSERT_SIZEOF(GStruct56, 0x14);

/// Extended view of the D_800827A0 BSS block for SPU voice indices at +0x3E/+0x3F
/// (used by func_800569D4 / func_80056E38). The zero-init in func_800574BC covers
/// 0x44 bytes from D_800827A0, so these offsets sit inside that block.
typedef struct _GStruct61 {
    /* 0x00 */ u8 pad[0x3E];
    /* 0x3E */ s8 field_3E;
    /* 0x3F */ s8 field_3F;
} GStruct61;
STATIC_ASSERT_SIZEOF(GStruct61, 0x40);

/// "oneA" (0x41656E6F) tagged chunk header read by func_8005664C.
/// Located at a signed byte offset into a raw buffer.
typedef struct _GStruct58 {
    /* 0x0 */ s32 field_0;
    /* 0x4 */ u16 field_4;
    /* 0x6 */ u16 field_6;
} GStruct58;
STATIC_ASSERT_SIZEOF(GStruct58, 0x8);

/// Destination for func_8005664C: receives halfwords from a GStruct58 chunk.
typedef struct _GStruct59 {
    /* 0x00 */ u8  pad_00[0x3A];
    /* 0x3A */ u16 field_3A;
    /* 0x3C */ u16 field_3C;
} GStruct59;
STATIC_ASSERT_SIZEOF(GStruct59, 0x3E);

/// Dialog / prompt descriptor used by 21FDC.c handlers (e.g. func_80036E78,
/// func_80036D98, func_80036B2C). field_8 is a signed menu/option index passed
/// to rendering helpers; field_B is a flag written on the alternate confirm
/// path; field_C is a selection/confirm flag (1 = confirm); field_18/field_1A
/// are position halfwords; field_1C is data passed through to func_8002FDCC;
/// field_22 is a state halfword set with the alternate confirm path.
typedef struct _GStruct60 {
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
} GStruct60;

/// Linked text option node walked by func_80049AF0 (index via GStruct60::field_8).
/// field_0 is the string passed to func_8002FDCC; field_4 is the next node.
typedef struct _GStruct69 {
    /* 0x0 */ u8*                field_0;
    /* 0x4 */ struct _GStruct69* field_4;
} GStruct69;
STATIC_ASSERT_SIZEOF(GStruct69, 0x8);

/// Context at Task::field_34 for the func_80049AF0 dialog path.
/// field_4 is the head of a GStruct69 list; field_C bit0 gates cancel input.
typedef struct _GStruct70 {
    /* 0x00 */ byte       unknown_0[4];
    /* 0x04 */ GStruct69* field_4;
    /* 0x08 */ byte       unknown_8[4];
    /* 0x0C */ u8         field_C;
} GStruct70;

/// Context at Task::field_34 for the func_80049C00 UI path.
/// field_0 is a base index copied into UiList field_4/field_5; field_2 receives
/// the selected index from UiObject::field_2C on confirm/cancel; field_8 is an
/// optional string passed to func_80047F40.
typedef struct _GStruct73 {
    /* 0x00 */ u8    field_0;
    /* 0x01 */ byte  pad_1;
    /* 0x02 */ s16   field_2;
    /* 0x04 */ byte  pad_4[4];
    /* 0x08 */ char* field_8;
} GStruct73;

/// 0x18-byte voice-slot lookup result filled by func_80054D58 and consumed by
/// func_800558E8 / func_80055EF8. field_0 is the chosen slot index (or error);
/// field_1..field_6 are candidate slot indices (-1 = empty); field_7 is the
/// candidate count; field_8/C/10/14 hold ranking scores / IDs.
typedef struct _GStruct66 {
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
} GStruct66;
STATIC_ASSERT_SIZEOF(GStruct66, 0x18);

/// 0xC-byte init-table entry at D_80068A60 (two entries used by func_80053FF4).
/// field_0 indexes D_800680AC for a slot id; field_2 is written to GStruct31.field_8
/// and GStruct42.field_8; field_4/field_6 are F3D458_Malloc sizes; field_8 is stored
/// to GStruct31.field_C.
typedef struct _GStruct75 {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
    /* 0x4 */ u16 field_4;
    /* 0x6 */ u16 field_6;
    /* 0x8 */ s32 field_8;
} GStruct75;
STATIC_ASSERT_SIZEOF(GStruct75, 0xC);

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
