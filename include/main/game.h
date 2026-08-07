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

// Module headers (types + public APIs).
#include "main/task.h"
#include "main/display.h"
#include "main/pad.h"
#include "main/mc.h"
#include "main/boot.h"
#include "main/gamemain.h"
#include "main/mem.h"
#include "main/sound.h"
#include "main/cdaudio.h"
#include "main/cdstream.h"
#include "main/stream.h"
#include "main/text.h"
#include "main/ui.h"
#include "main/tmd.h"
#include "main/fs.h"

// Types — game session / stage / shared helpers

/// WIP: boot/gamemain flag block (Wip_SysFlags). field_4 set on soft-reset paths;
/// field_6 polled/cleared in boot and F344. Role not fully proven.
typedef struct _WipSysFlags {
    byte unknown_0[0x4];
    s16  field_4;
    s16  field_6;
    byte unknown_8[0x18];
} WipSysFlags;
STATIC_ASSERT_SIZEOF(WipSysFlags, 0x20);
/// 4-byte block assigned via unaligned lwl/lwr (see Snd_ApplyVolumeTable).
typedef struct _GBytes4 {
    u8 data[4];
} GBytes4;
/// 6-byte block assigned via unaligned lwl/lwr + lb/sb (see TaskIdMap_RemapIndex).
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
