#ifndef SESSION_H
#define SESSION_H

#include "common.h"

// =============================================================================
// Types — game session / actor objects (Game_Session)
// =============================================================================

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

// =============================================================================
// Globals
// =============================================================================

/// Active game session object (data segment / BSS pointer).
extern GameSession* Game_Session;

// --- APIs (from unknown_syms) ---
void Game_ClearSession(void);
void Game_ClearEd68(void);

#endif // SESSION_H
