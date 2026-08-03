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
    byte              unknown_2c[4];
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
    byte    unknown_102[0x9];
    u8      field_10b;
    byte    unknown_10c[0x1];
    u8      field_10d;
    byte    unknown_10e[0x4];
    s16     field_112;
    byte    unknown_114[0xC];
    s16     field_120;
    byte    unknown_122[0x2];
    u16     field_124;
    byte    unknown_126[0x4];
    u16     field_12a;
    byte    unknown_12c[0x4];
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
    byte          unknown_55[0x175];
    u16           field_1ca;
    byte          unknown_1cc[0x38];
    u16           field_204;
    byte          unknown_206[0x8];
    s16           field_20E;
    byte          unknown_210[0x14];
    u16           field_224;
    byte          unknown_226[0xE];
    s16           field_234;
    byte          unknown_236[0x1C];
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
    byte  unknown_0[0xC];
    void* field_C[16]; // 0xC..0x4B; cleared by func_8002D780
    byte  unknown_4C[0xF0];
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
} GStruct16From4;
STATIC_ASSERT_SIZEOF(GStruct16From4, 0x8);

typedef struct _GStruct16 {
    s16                field_0;
    s16                field_2;
    u8                 field_4;
    u8                 field_5;
    u16                field_6;
    s32                field_8;
    byte               unknown_C[0x8];
    struct _GStruct16* field_14;
    struct _GStruct16* field_18;
} GStruct16;
STATIC_ASSERT_SIZEOF(GStruct16, 0x1C);

/// Pointed to by D_80062698 (bss object D_8007A320, size 0x38).
typedef struct _GStruct17 {
    /* 0x00 */ byte unknown_0[0x11];
    /* 0x11 */ u8   field_11;
    /* 0x12 */ u8   field_12;
    /* 0x13 */ byte unknown_13[0x2];
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

/// BSS object D_80082818 (size 0x58). Also matches D_800828F0 layout.
typedef struct _GStruct19 {
    byte unknown_0[0x6];
    s16  field_6;
    byte unknown_8[0x50];
} GStruct19;
STATIC_ASSERT_SIZEOF(GStruct19, 0x58);

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

/// Second argument to memcard/save state handlers in 21FDC.c (e.g. func_80035AD4).
/// Larger object; only fields used so far are named.
typedef struct _GStruct21 {
    /* 0x00 */ byte unknown_0[0x24];
    /* 0x24 */ s32  field_24;
    /* 0x28 */ s32  field_28;
} GStruct21;

/// 8-byte slot at GStruct22::field_484 (16 entries, indexed by opcode low nibble).
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

/// 4-byte entry at D_8007EB98 (see func_8004E5A0).
typedef struct _GStruct26 {
    s16 field_0;
    s16 field_2;
} GStruct26;
STATIC_ASSERT_SIZEOF(GStruct26, 0x4);

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
    /* 0x10 */ void*              field_10;
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
