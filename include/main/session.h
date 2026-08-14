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
    byte  unknown_82[0xAA];
    u8    field_12C;
    byte  unknown_12D[0xF];
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

struct _Task;

/// Large object pointed to by Task::idMap for the slot-3 game object
/// (Game_GetPtrSlot(3)). Sparse fields used by Display_SpawnFromMode.
typedef struct _GameActor {
    /* 0x000 */ byte          pad_0[0x90];
    /* 0x090 */ s32           field_90;
    /* 0x094 */ byte          pad_94[0x18];
    /* 0x0AC */ byte          field_AC[0x20]; // 0x20-byte list node (func_800E1638)
    /* 0x0CC */ byte          field_CC[0x20];
    /* 0x0EC */ byte          field_EC[0x20];
    /* 0x10C */ byte          field_10C[0x20];
    /* 0x12C */ byte          field_12C[0x20];
    /* 0x14C */ byte          pad_14C[0x30];
    /* 0x17C */ byte          field_17C; // address taken for func_800E1A6C
    /* 0x17D */ byte          pad_17D[0x797];
    /* 0x914 */ struct _Task* field_914;
    /* 0x918 */ struct _Task* field_918;
    /* 0x91C */ struct _Task* field_91C;
    /* 0x920 */ struct _Task* field_920;
    /* 0x924 */ struct _Task* field_924;
    /* 0x928 */ byte          pad_928[0x8];
    /* 0x930 */ byte          field_930; // address taken for func_801011D0
    /* 0x931 */ byte          pad_931[0x13];
    /* 0x944 */ s16           field_944;
    /* 0x946 */ s16           field_946;
    /* 0x948 */ s16           field_948;
    /* 0x94A */ s16           field_94A;
    /* 0x94C */ s16           field_94C;
    /* 0x94E */ s16           field_94E;
    /* 0x950 */ s16           field_950;
    /* 0x952 */ byte          pad_952[4];
    /* 0x956 */ u16           field_956;
    /* 0x958 */ s16           field_958;
    /* 0x95A */ byte          pad_95A[0x12];
    /* 0x96C */ s16           field_96C;
    /* 0x96E */ s16           field_96E;
    /* 0x970 */ s16           field_970;
    /* 0x972 */ u8            field_972;
    /* 0x973 */ byte          pad_973[7];
    /* 0x97A */ u8            field_97A;
    /* 0x97B */ byte          pad_97B[7];
    /* 0x982 */ s8            field_982;
    /* 0x983 */ byte          pad_983;
    /* 0x984 */ u8            field_984;
    /* 0x985 */ byte          pad_985[0x8];
    /* 0x98D */ u8            field_98D;
    /* 0x98E */ u8            field_98E;
    /* 0x98F */ byte          pad_98F;
    /* 0x990 */ u8            field_990;
} GameActor;
STATIC_ASSERT_SIZEOF(GameActor, 0x994);

/// Object pointed to by Task::extra; field_8 is a s32* cleared by
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
extern GameSession  D61CC0_800714C0;

/// Session pointer-slot table on Game_Session (field_C[16]).
void  Game_SetPtrSlot(void* ptr, s32 index);
void* Game_GetPtrSlot(s32 index);
void  Game_ClearPtrSlots(void);
void  Game_ClearSession(void);
void  Game_ClearEd68(void);

extern s32 D_8005ED68;
extern s32 D_8005ED8C;

#endif // SESSION_H
