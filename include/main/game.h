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

// Type forwards declarations.
struct _GStruct0;

/// Callback function in a `_GStruct0`.
typedef void (*GFunc0)(struct _GStruct0*);

/// Intrusive linked list node for a `GStruct0`.
///
/// The head node is not an element in the linked list and points to the first
/// and the last elements.
typedef struct _GStruct0Node {
    struct _GStruct0*     next;
    struct _GStruct0Node* prev;
} GStruct0Node;

typedef struct _GStruct0 {
    GStruct0Node      node;
    struct _GStruct0* field_8;
    struct _GStruct0* field_c;
    struct _GStruct0* field_10;
    GFunc0            field_14;
    GFunc0            field_18;
    byte              unknown1[4];
    void*             field_20;
    byte              unknown_24[4];
    u8                field_28;
    byte              unknown_29;
    s16               field_2a;
    void*             field_2c;
    s32               field_30;
    byte              unknown_34[0x14];
} GStruct0;
STATIC_ASSERT_SIZEOF(GStruct0, 0x48);

typedef struct _GStruct1 {
    s32     field_0;
    s32     field_4;
    s32     field_8;
    s32     field_c;
    s32     field_10;
    s32     field_14;
    byte    unknown_18[0x5];
    s8      field_1d;
    u8      field_1e;
    u8      field_1f;
    DISPENV field_20[2];
    DRAWENV field_48[2];
    u8      field_100;
    u8      field_101;
    byte    unknown_102[0x1];
    u8      field_103;
    byte    unknown_104[0x6];
    u8      field_10a;
    u8      field_10b;
    byte    unknown_10c[0x1];
    u8      field_10d;
    byte    unknown_10e[0x4];
    s16     field_112;
    s32     field_114;
    s32     field_118;
    byte    unknown_11c[0x1];
    u8      field_11d;
    byte    unknown_11e[0x2];
    s16     field_120;
    byte    unknown_122[0x2];
    u16     field_124;
    s8      field_126;
    byte    unknown_127[0x3];
    u16     field_12a;
    u16     field_12c;
    byte    unknown_12e[0x2];
    u8      field_130;
    byte    unknown_131[0x7];
} GStruct1;
STATIC_ASSERT_SIZEOF(GStruct1, 0x138);

typedef struct _GStruct2 {
    u16    field_0;
    byte   unknown_2[0x2];
    GFunc0 field_4;
    byte   unknown_8[0x4];
} GStruct2;
STATIC_ASSERT_SIZEOF(GStruct2, 0xc);

typedef struct _GStruct3Entry {
    u8 field_0;
    u8 field_1;
    u8 field_2;
    u8 field_3;
    u8 field_4;
    u8 field_5;
    u8 field_6;
    u8 field_7;
} GStruct3Entry;
STATIC_ASSERT_SIZEOF(GStruct3Entry, 0x8);

typedef struct _GStruct3 {
    GStruct3Entry entries[8];
    GStruct3Entry field_40;
    byte          unknown_48[0x4];
    s8            field_4c;
    byte          unknown_4d[0x7];
    u8            field_54;
    byte          unknown_55[0x173];
    u16           field_1c8;
    u16           field_1ca;
    byte          unknown_1cc[0x4];
    u16           field_1d0;
    u16           field_1d2;
    u16           field_1d4;
    byte          unknown_1d6[0x10];
    s16           field_1E6;
    byte          unknown_1E8[0x2];
    s16           field_1EA;
    byte          unknown_1EC[0x10];
    u16           field_1fc;
    u8            field_1FE;
    u8            field_1FF;
    u16           field_200;
    u16           field_202;
    u16           field_204;
    byte          unknown_206[0x8];
    s16           field_20E;
    byte          unknown_210[0x4];
    u16           field_214;
    byte          unknown_216[0x4];
    s16           field_21A;
    byte          unknown_21C[0x8];
    u16           field_224;
    byte          unknown_226[0x2];
    u16           field_228;
    byte          unknown_22a[0xA];
    s16           field_234;
    s16           field_236;
    byte          unknown_238[0x1A];
    s16           field_252;
} GStruct3;
STATIC_ASSERT_SIZEOF(GStruct3, 0x254);

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
    byte unknown_0[0x6];
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

typedef struct _GStruct11 {
    u32 buffers[20][1920];
} GStruct11;
STATIC_ASSERT_SIZEOF(GStruct11, 0x25800);

typedef struct _GStruct13 {
    u16 field_0;
    u16 field_2;
    u32 field_4;
} GStruct13;
STATIC_ASSERT_SIZEOF(GStruct13, 0x8);

typedef struct _GStruct14 {
    byte  unknown_0[0x4];
    byte  field_4; // address taken by func_8001D39C
    byte  unknown_5;
    u8    field_6;
    u8    field_7;
    byte  unknown_8[0x4];
    void* field_C[16]; // 0xC..0x4B; cleared by func_8002D780
    byte  unknown_4C[0x30];
    s16   field_7C;
    byte  unknown_7E[0xBE];
} GStruct14;
STATIC_ASSERT_SIZEOF(GStruct14, 0x13C);

typedef struct _GStruct15 {
    byte unknown_0[0x2];
    u8   field_2;
    u8   field_3;
} GStruct15;
STATIC_ASSERT_SIZEOF(GStruct15, 0x4);

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
    /* 0x00 */ byte unknown_0[0xC];
    /* 0x0C */ u32  field_C;
    /* 0x10 */ byte unknown_10;
    /* 0x11 */ u8   field_11;
    /* 0x12 */ u8   field_12;
    /* 0x13 */ u8   field_13;
    /* 0x14 */ u8   field_14;
    /* 0x15 */ u8   field_15;
    /* 0x16 */ byte unknown_16;
    /* 0x17 */ u8   field_17;
    /* 0x18 */ byte unknown_18[0x2];
    /* 0x1A */ u8   field_1a;
    /* 0x1B */ byte unknown_1b;
    /* 0x1C */ u32  field_1c;
    /* 0x20 */ byte unknown_20[0x18];
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
    /* 0x0C */ byte unknown_C[0x40];
    /* 0x4C */ s16  field_4C;
    /* 0x4E */ byte unknown_4E[0x5];
    /* 0x53 */ u8   field_53;
    /* 0x54 */ u16  field_54;
    /* 0x56 */ byte unknown_56[0x2];
} GStruct19;
STATIC_ASSERT_SIZEOF(GStruct19, 0x58);

/// BSS object D_800828F0 (size 0x58). CD ready-callback install state.
typedef struct _GStruct32 {
    /* 0x00 */ u8    field_0;
    /* 0x01 */ u8    field_1;
    /* 0x02 */ u8    field_2;
    /* 0x03 */ u8    field_3;
    /* 0x04 */ void* field_4;
    /* 0x08 */ byte  unknown_8[0x50];
} GStruct32;
STATIC_ASSERT_SIZEOF(GStruct32, 0x58);

/// Object passed to func_80048C10 / func_80048D58 (e.g. via GStruct0::field_20).
typedef struct _GStruct20 {
    /* 0x00 */ s32  field_0;
    /* 0x04 */ byte unknown_4[0x18];
    /* 0x1C */ s16  field_1c;
    /* 0x1E */ s16  unknown_1e;
    /* 0x20 */ s16  field_20;
    /* 0x22 */ s16  field_22;
} GStruct20;
STATIC_ASSERT_SIZEOF(GStruct20, 0x24);

/// Object at GStruct0::field_20 used by func_80048838 / func_80033F6C.
/// field_0 is a status flag; field_8 is set to 3 when torn down; field_28 is a
/// parent/context pointer (has field_c for the recursive walk in func_80048838).
typedef struct _GStruct37 {
    /* 0x00 */ s32   field_0;
    /* 0x04 */ byte  unknown_4[0x4];
    /* 0x08 */ s32   field_8;
    /* 0x0C */ byte  unknown_C[0x1C];
    /* 0x28 */ void* field_28;
} GStruct37;

/// Second argument to memcard/save state handlers in 21FDC.c (e.g. func_80035AD4,
/// func_80034070, func_80035FD8). Larger object; only fields used so far are named.
/// field_10/field_14 are MemCardSync cmds/rslt outs.
typedef struct _GStruct21 {
    /* 0x000 */ s32  field_0;
    /* 0x004 */ s32  field_4;
    /* 0x008 */ s32  field_8;
    /* 0x00C */ s32  field_C;
    /* 0x010 */ s32  field_10;
    /* 0x014 */ s32  field_14;
    /* 0x018 */ s32  field_18;
    /* 0x01C */ byte unknown_1C[0x8];
    /* 0x024 */ s32  field_24;
    /* 0x028 */ s32  field_28;
    /* 0x02C */ s32  field_2C;
    /* 0x030 */ byte unknown_30[0x9E8];
    /* 0xA18 */ s32  field_A18;
    /* 0xA1C */ s32  field_A1C;
    /* 0xA20 */ s32  field_A20;
} GStruct21;

/// 8-byte slot at GStruct22::field_484 (16 entries, indexed by opcode low nibble).
/// field_0 is written as a full word (0x407F4000) by func_800528BC; field_4 is a byte.
typedef struct _GStruct22Entry {
    /* 0x0 */ s32 field_0;
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

/// BSS object D_80072168. Large; only fields used so far are named.
/// Accessed up to at least 0x942 (see func_80033D3C).
typedef struct _GStruct23 {
    /* 0x000 */ byte unknown_0[0x21];
    /* 0x021 */ u8   field_21;
    /* 0x022 */ byte unknown_22[0x3];
    /* 0x025 */ u8   field_25;
    /* 0x026 */ byte unknown_26[0x182];
    /* 0x1A8 */ u8   field_1a8;
    /* 0x1A9 */ u8   field_1a9;
    /* 0x1AA */ u8   field_1aa;
    /* 0x1AB */ u8   field_1ab;
} GStruct23;

/// Element of BSS array D_8006D4F0 (15 entries, total 0x258).
/// Fields inferred from F344.c accessors (func_8001ED20, func_8001EED8, etc.).
typedef struct _GStruct24 {
    /* 0x00 */ s16  field_0;
    /* 0x02 */ byte unknown_2[0x2];
    /* 0x04 */ s32  field_4;
    /* 0x08 */ byte unknown_8[0x4];
    /* 0x0C */ u16  field_C;
    /* 0x0E */ u16  field_E;
    /* 0x10 */ byte unknown_10[0x2];
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

/// Element of BSS array D_80071620 (2 entries, total 0xB8).
/// Indexed with stride 0x5C (see func_8002C9B0). field_A is a counter/flag
/// written by pad-related setup (func_8002C9B0, func_8003FCF8, func_8003FC8C).
typedef struct _GStruct25 {
    /* 0x00 */ byte unknown_0[0xA];
    /* 0x0A */ u8   field_A;
    /* 0x0B */ byte unknown_B[0x51];
} GStruct25;
STATIC_ASSERT_SIZEOF(GStruct25, 0x5C);

/// UI list/menu object (data symbols D_8006116C, D_80061194; size 0x24).
/// field_0 is a function-table pointer; field_9 / field_A / field_10 are list
/// cursor / flag / selection index used by func_80036A70 / func_80036C04 /
/// func_800489A0 / func_8004917C.
typedef struct _GStruct46 {
    /* 0x00 */ byte unknown_0[0x9];
    /* 0x09 */ u8   field_9;
    /* 0x0A */ u8   field_A;
    /* 0x0B */ byte unknown_B[0x5];
    /* 0x10 */ s32  field_10;
    /* 0x14 */ byte unknown_14[0x10];
} GStruct46;
STATIC_ASSERT_SIZEOF(GStruct46, 0x24);

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
/// Same layout as GStruct0Node: next is the first element, prev is the last
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
/// Cleared by func_80028718 during system init.
extern s32 D_80071210;

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
/// field_8 is a small integer state; field_16 is a signed counter/timer;
/// field_24 is a callback invoked with the second handler argument.
typedef struct _GStruct30 {
    /* 0x00 */ byte unknown_0[0x8];
    /* 0x08 */ s32  field_8;
    /* 0x0C */ byte unknown_C[0xA];
    /* 0x16 */ s16  field_16;
    /* 0x18 */ byte unknown_18[0xC];
    /* 0x24 */ void (*field_24)(void*);
} GStruct30;

/// Header for the bank table blob pointed to by GStruct31.field_0.
/// field_4 is the bank ID (high halfword remapped by func_80053F00 when the
/// request high nibble is 0x1); field_6 is the entry count used by func_8005414C.
/// A u16 offset table follows at 0x8.
typedef struct _GStruct45 {
    /* 0x0 */ u8  unknown_0[4];
    /* 0x4 */ u16 field_4;
    /* 0x6 */ u16 field_6;
} GStruct45;
STATIC_ASSERT_SIZEOF(GStruct45, 0x8);

/// 16-byte slot in D_80082148[16] (BSS size 0x100). Indexed by func_800561C0
/// and related helpers in 43FFC.c / 410B0.c.
typedef struct _GStruct31 {
    /* 0x0 */ GStruct45* field_0;
    /* 0x4 */ void*      field_4;
    /* 0x8 */ s32        field_8;
    /* 0xC */ void*      field_C;
} GStruct31;
STATIC_ASSERT_SIZEOF(GStruct31, 0x10);

/// Voice/FX object carved from D_80082148 with stride 0x40 (func_80056240).
/// field_0 is the SPU voice index; field_8 is a small state flag.
/// func_80056068 walks link fields at 0x34/0x38/0x3C on free.
typedef struct _GStruct43 {
    /* 0x00 */ s8  field_0;
    /* 0x01 */ u8  unknown_01[0x7];
    /* 0x08 */ s16 field_8;
    /* 0x0A */ u8  unknown_0A[0x2A];
    /* 0x34 */ u8  unknown_34[0xC];
} GStruct43;
STATIC_ASSERT_SIZEOF(GStruct43, 0x40);

/// State block at D_800820F0; field_3 is also D_800820F3.
/// field_14/field_18 cleared by func_800537FC; field_10 sized by func_8005363C.
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
} GStruct34;
STATIC_ASSERT_SIZEOF(GStruct34, 0x1C);

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
typedef struct _GStruct36Entry {
    /* 0x00 */ u8 unknown_00[5];
    /* 0x05 */ u8 field_5;
    /* 0x06 */ u8 unknown_06[0x36];
} GStruct36Entry;
STATIC_ASSERT_SIZEOF(GStruct36Entry, 0x3C);

/// State block at D_8007F300 (logical stride 0x5DC; BSS allocation 0x5E0).
/// field_0 is status; field_3 is the number of track entries starting at 0x4C.
/// field_8 is a scaled volume; field_C is a sentinel (0xFFFF when cleared).
typedef struct _GStruct36 {
    /* 0x00 */ u8             field_0;
    /* 0x01 */ u8             field_1;
    /* 0x02 */ u8             field_2;
    /* 0x03 */ u8             field_3;
    /* 0x04 */ u8             unknown_04[4];
    /* 0x08 */ s16            field_8;
    /* 0x0A */ u8             unknown_0A[2];
    /* 0x0C */ s32            field_C;
    /* 0x10 */ u8             unknown_10[0x3C];
    /* 0x4C */ GStruct36Entry entries[1];
    /* 0x88 */ u8             unknown_88[0x5DC - 0x88];
} GStruct36;
STATIC_ASSERT_SIZEOF(GStruct36, 0x5DC);

/// Text-measure / draw-request block passed to func_8002EDFC / func_8002E53C.
/// field_0 / field_2 are position (or accumulate measured width); field_C selects
/// the glyph table; field_D selects centering mode (1 = half width, 2 = full).
typedef struct _GStruct38 {
    /* 0x00 */ s16 field_0;
    /* 0x02 */ s16 field_2;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s8  field_C;
    /* 0x0D */ s8  field_D;
    /* 0x0E */ s8  field_E;
    /* 0x0F */ s8  pad_F;
} GStruct38;
STATIC_ASSERT_SIZEOF(GStruct38, 0x10);

/// BSS object D_80082758 (size 0x18). CD/audio stream state for 46FE4.c.
typedef struct _GStruct39 {
    /* 0x00 */ u8  field_0;
    /* 0x01 */ u8  field_1;
    /* 0x02 */ u8  field_2;
    /* 0x03 */ u8  pad_3;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
    /* 0x10 */ s32 field_10;
    /* 0x14 */ s32 field_14;
} GStruct39;
STATIC_ASSERT_SIZEOF(GStruct39, 0x18);

/// BSS object D_80073B88 (size 0x80). Initialized by func_8004C4D0.
/// field_18..field_1e are four s16 values set to 100; field_21/field_26 are flags.
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
    /* 0x27 */ byte unknown_27[0x59];
} GStruct40;
STATIC_ASSERT_SIZEOF(GStruct40, 0x80);

/// 0x14-byte sound/note entry indexed by func_8004EA60.
/// Callers read field_1/field_3/field_4/field_5/field_A/field_B/field_10.
typedef struct _GStruct41 {
    /* 0x00 */ u8  field_0;
    /* 0x01 */ u8  field_1;
    /* 0x02 */ u8  pad_2;
    /* 0x03 */ u8  field_3;
    /* 0x04 */ u8  field_4;
    /* 0x05 */ u8  field_5;
    /* 0x06 */ byte unknown_6[0x4];
    /* 0x0A */ u8  field_A;
    /* 0x0B */ u8  field_B;
    /* 0x0C */ byte unknown_C[0x4];
    /* 0x10 */ s32 field_10;
} GStruct41;
STATIC_ASSERT_SIZEOF(GStruct41, 0x14);

/// Sound bank header used by func_8004EA60 (and D_8007E0D8 entries, stride 0x20).
/// field_4 is the base of GStruct41 entries; field_10 is a u16 index table.
/// field_8 high nibble 0xF marks the slot free/invalid (see func_8004D0A0 / func_8004D0F0).
/// field_1C is a heap allocation released by func_8004D0F0.
typedef struct _GStruct42 {
    /* 0x00 */ void*      field_0;
    /* 0x04 */ GStruct41* field_4;
    /* 0x08 */ u16        field_8;
    /* 0x0A */ byte       unknown_A[0x6];
    /* 0x10 */ u16*       field_10;
    /* 0x14 */ void*      field_14;
    /* 0x18 */ byte       unknown_18[0x4];
    /* 0x1C */ void*      field_1C;
} GStruct42;
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

extern int      D_80068F98;
extern int      MainPadding;
extern GStruct3 D_80068FA0;
extern u8*      D_800691F4;
extern size_t   D_800691F8;

/// Game entry point. Called by `main`.
void GameMain(void);

#endif
