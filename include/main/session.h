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
    s8    field_0;
    s8    field_1;
    u8    field_2;
    byte  unknown_3;
    byte  field_4; // address taken by CdCmd_BuildVlcIfStream
    u8    field_5; // 1-based index into D_8010CB40 / D_8010CBA4 innermost tables; D_8010CB54 second level
    u8    field_6;
    u8    field_7;
    byte  unknown_8[0x4];
    void* field_C[16]; // 0xC..0x4B; cleared by Game_ClearPtrSlots
    u8    field_4C;
    u8    field_4D;
    u16   field_4E; // set to 1 by Fs_LoadFile for category-8 file ids
    byte  unknown_50[2];
    s16   field_52;
    byte  unknown_54[0xB];
    u8    field_5F; // skip-gate for func_800E74EC overlay-wait setup
    byte  unknown_60[6];
    u8    field_66; // 1: func_800CE3B4 uses D_8010EB94 + Ui_Scale15(2)
    byte  unknown_67;
    u8    field_68; // set/cleared by func_800E7378 / func_800E73E8 / func_800E7434
    u8    field_69; // bit 0x2: skip SndEvt_EnqueueType2(0, 0xB4) on last GpStateF0 ref
    byte  unknown_6A[0xA];
    u8    field_74;
    byte  unknown_75;
    s16   field_76; // set: func_800AD378 rebuilds via func_800ACD2C
    byte  unknown_78[4];
    s16   field_7C;
    s16   field_7E;
    s16   field_80;
    byte  unknown_82[0xA4];
    u8    field_126;
    byte  unknown_127;
    u8    field_128; // 0xFF sentinel in func_800B0748 / func_800B082C
    byte  unknown_129[3];
    u8    field_12C;
    byte  unknown_12D[0xC];
    u8    field_139;
    byte  unknown_13A;
    u8    field_13B;
} GameSession;
STATIC_ASSERT_SIZEOF(GameSession, 0x13C);

/// Overlay of `GameSession` starting at offset 0x4 (`field_4`..`field_7`).
/// Used when the compiler keeps `&Game_Session->field_4` in a register.
typedef struct _GameSessionFrom4 {
    /* 0x0 */ u8 field_0; // GameSession.field_4
    /* 0x1 */ u8 field_1; // GameSession.field_5
    /* 0x2 */ u8 field_2; // GameSession.field_6
    /* 0x3 */ u8 field_3; // GameSession.field_7
} GameSessionFrom4;
STATIC_ASSERT_SIZEOF(GameSessionFrom4, 0x4);

struct _Task;
struct _GpLinkNode;
struct _GpActorD4;

/// 0x18 record wiped by `func_800E18E0`. That helper zeros `count` entries
/// and writes 2 to the last element's `field_0`. `field_0` bit 0x1 marks an
/// occupied slot; bit 0x2 marks the last element. `func_800E19B8` returns the
/// 1-based index of the last occupied slot whose `field_4` equals `arg1`,
/// or 1 as soon as any occupied slot is seen when `arg1` is 0.
/// `func_800E1A1C` counts occupied slots whose `field_4` high 16 bits match
/// `arg1`. `func_800E1A6C` walks until bit 0x2, and for each occupied slot
/// keeps only that last-element bit and zeros the payload halfwords / word
/// (leaving 0xE and 0x16 untouched). Embedded as `GameActor.field_17C[18]`;
/// `func_801041B4` tests `field_4` bits 0x100100.
typedef struct _GpRec18 {
    /* 0x00 */ u16  field_0;
    /* 0x02 */ s16  field_2;
    /* 0x04 */ s32  field_4;
    /* 0x08 */ s16  field_8;
    /* 0x0A */ s16  field_A;
    /* 0x0C */ s16  field_C;
    /* 0x0E */ byte pad_E[2];
    /* 0x10 */ s16  field_10;
    /* 0x12 */ s16  field_12;
    /* 0x14 */ s16  field_14;
    /* 0x16 */ byte pad_16[2];
} GpRec18;
STATIC_ASSERT_SIZEOF(GpRec18, 0x18);

/// 0x28-byte record in `GameActor.field_448`. `field_0` is a flag halfword
/// (bit 0x100: `func_8010583C`; bits 0x102: `func_80105894`). Count is
/// `GameActor.field_938` (init 0x13). Slid-actor overlay: `func_801058BC`
/// stores a clamped 1..0x7F byte at `GameActor.field_441`.
typedef struct _GameActorSlot {
    /* 0x00 */ u16  field_0;
    /* 0x02 */ byte pad_2[0x26];
} GameActorSlot;
STATIC_ASSERT_SIZEOF(GameActorSlot, 0x28);

/// Large object pointed to by Task::idMap for the slot-3 game object
/// (Game_GetPtrSlot(3)). Sparse fields used by Display_SpawnFromMode.
typedef struct _GameActor {
    /* 0x000 */ byte                pad_0[0x40];
    /* 0x040 */ s32                 field_40;
    /* 0x044 */ s32                 field_44;
    /* 0x048 */ s32                 field_48;
    /* 0x04C */ byte                pad_4C[4];
    /* 0x050 */ s16                 field_50; // SVECTOR.vx; func_80104D68 / RotMatrix
    /* 0x052 */ s16                 field_52; // facing angle (lh); func_8010BCF4 / func_80103E7C
    /* 0x054 */ s16                 field_54; // SVECTOR.vz; func_80104D68 / RotMatrix
    /* 0x056 */ byte                pad_56[0x3A];
    /* 0x090 */ s32                 field_90;
    /* 0x094 */ byte                pad_94[0x18];
    /* 0x0AC */ byte                field_AC[0x20]; // 0x20-byte list node (func_800E1638)
    /* 0x0CC */ byte                field_CC[0x20];
    /* 0x0EC */ byte                field_EC[0x20];
    /* 0x10C */ byte                field_10C[0x18]; // 0x20-byte list node; field_124 is +0x18
    /* 0x124 */ u32                 field_124;
    /* 0x128 */ byte                pad_128[4];
    /* 0x12C */ byte                field_12C[0x20];
    /* 0x14C */ byte                pad_14C[0x30];
    /* 0x17C */ GpRec18             field_17C[18];   // func_800E1A6C / func_801041B4
    /* 0x32C */ byte                pad_32C[0xF8];
    /* 0x424 */ byte                field_424[0x14]; // GpAnimCtx overlay; func_800B4514
    /* 0x438 */ byte                pad_438[9];      // GpAnimSlot array base; func_80105B0C
    /* 0x441 */ u8                  field_441;       // slid-actor overlay; see GameActorSlot
    /* 0x442 */ byte                pad_442[6];
    /* 0x448 */ GameActorSlot       field_448[19];
    /* 0x740 */ byte                pad_740[0x1CC];
    /* 0x90C */ struct _GpLinkNode* field_90C;
    /* 0x910 */ struct _GpActorD4*  field_910;
    /* 0x914 */ struct _Task*       field_914;
    /* 0x918 */ struct _Task*       field_918;
    /* 0x91C */ struct _Task*       field_91C;
    /* 0x920 */ struct _Task*       field_920;
    /* 0x924 */ struct _Task*       field_924;
    /* 0x928 */ byte                pad_928[0x8];
    /* 0x930 */ byte                field_930; // address taken for func_801011D0
    /* 0x931 */ byte                pad_931[3];
    /* 0x934 */ s32                 field_934;
    /* 0x938 */ s16                 field_938; // GameActorSlot count (init 0x13)
    /* 0x93A */ byte                pad_93A[2];
    /* 0x93C */ u16                 field_93C;
    /* 0x93E */ s16                 field_93E;
    /* 0x940 */ s16                 field_940;
    /* 0x942 */ s16                 field_942;
    /* 0x944 */ s16                 field_944;
    /* 0x946 */ s16                 field_946;
    /* 0x948 */ s16                 field_948;
    /* 0x94A */ s16                 field_94A;
    /* 0x94C */ s16                 field_94C;
    /* 0x94E */ s16                 field_94E;
    /* 0x950 */ s16                 field_950;
    /* 0x952 */ byte                pad_952[2];
    /* 0x954 */ u16                 field_954;
    /* 0x956 */ u16                 field_956;
    /* 0x958 */ s16                 field_958;
    /* 0x95A */ u16                 field_95A;
    /* 0x95C */ u16                 field_95C;
    /* 0x95E */ u16                 field_95E;
    /* 0x960 */ u16                 field_960;
    /* 0x962 */ u16                 field_962;
    /* 0x964 */ byte                pad_964[2];
    /* 0x966 */ u16                 field_966;
    /* 0x968 */ byte                pad_968[4];
    /* 0x96C */ s16                 field_96C;
    /* 0x96E */ s16                 field_96E;
    /* 0x970 */ s16                 field_970;
    /* 0x972 */ u8                  field_972;
    /* 0x973 */ s8                  field_973;
    /* 0x974 */ s8                  field_974;
    /* 0x975 */ s8                  field_975;
    /* 0x976 */ s8                  field_976;
    /* 0x977 */ s8                  field_977;
    /* 0x978 */ s8                  field_978;
    /* 0x979 */ byte                pad_979;
    /* 0x97A */ u8                  field_97A;
    /* 0x97B */ byte                pad_97B;
    /* 0x97C */ s8                  field_97C;
    /* 0x97D */ u8                  field_97D;
    /* 0x97E */ u8                  field_97E;
    /* 0x97F */ s8                  field_97F;
    /* 0x980 */ byte                pad_980;
    /* 0x981 */ u8                  field_981;
    /* 0x982 */ s8                  field_982;
    /* 0x983 */ u8                  field_983;
    /* 0x984 */ u8                  field_984;
    /* 0x985 */ u8                  field_985;
    /* 0x986 */ byte                pad_986;
    /* 0x987 */ u8                  field_987;
    /* 0x988 */ u8                  field_988;
    /* 0x989 */ u8                  field_989;
    /* 0x98A */ u8                  field_98A;
    /* 0x98B */ u8                  field_98B;
    /* 0x98C */ u8                  field_98C;
    /* 0x98D */ u8                  field_98D;
    /* 0x98E */ u8                  field_98E;
    /* 0x98F */ byte                pad_98F;
    /* 0x990 */ u8                  field_990;
    /* 0x991 */ s8                  field_991; // func_80109374 requires 0 to write field_97D = 1
    /* 0x992 */ byte                pad_992;
    /* 0x993 */ u8                  field_993;
} GameActor;
STATIC_ASSERT_SIZEOF(GameActor, 0x994);

/// Object pointed to by Task::extra; field_8 is a GsCOORDINATE2* (stored as
/// s32* so Display_SpawnFromMode can clear flg via *ptr = 0) after optional
/// func_801011D0 / func_800E1A6C setup. field_C flag bits are OR'd with 0x80
/// in Task_Kill (type-1 deferred kill).
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
