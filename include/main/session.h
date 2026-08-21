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

/// 18-byte MATRIX rotation (3x3 s16). Assigned via unaligned lwl/lwr + lh/sh
/// (see func_800A8A48). The trailing s16 (not u8[2]) keeps the last two bytes
/// a halfword; a pure u8[18] emits lb/sb instead.
typedef struct _GBytes18 {
    u8  data[0x10];
    s16 field_10;
} GBytes18;

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
    u8    field_5; // 1-based index into D_8010CB40 / D_8010CBA4 / D_8010CB7C innermost tables; D_8010CB54 second level
    u8    field_6;
    u8    field_7;
    u8    field_8;
    u8    field_9;
    byte  unknown_A[0x2];
    void* field_C[16]; // 0xC..0x4B; cleared by Game_ClearPtrSlots
    u8    field_4C;
    u8    field_4D;
    u16   field_4E; // set to 1 by Fs_LoadFile for category-8 file ids
    byte  unknown_50[2];
    s16   field_52;
    byte  unknown_54[4];
    u16   field_58; // current pad mask; func_80103804 copies this onto GameActor.field_962
    byte  unknown_5A[4];
    u8    field_5E; // set to 1 by func_8009FEDC before allocating the play-clock idMap
    u8    field_5F; // skip-gate for func_800E74EC overlay-wait setup
    byte  unknown_60[4];
    u8    field_64; // nonzero: func_800AD5B8 / func_800AD50C skip their state dispatch
    u8    field_65; // 1: func_800D8EA0 skips color-matrix rebuild unless GameActorExt.field_18 is set
    u8    field_66; // 1: func_800CE3B4 uses D_8010EB94 + Ui_Scale15(2)
    byte  unknown_67;
    u8    field_68; // set/cleared by func_800E7378 / func_800E73E8 / func_800E7434
    u8    field_69; // bit 0x1: skip bank-load spawn in func_800A0718; bit 0x2: skip bank-load spawn in func_800A110C (else skip SndEvt_EnqueueType2(0, 0xB4) on last GpStateF0 ref); bit 0x4: spawn arg 3 vs 2 in func_800A0718; bit 0x8: spawn arg 3 vs 1 in func_800A110C
    byte  unknown_6A[0xA];
    u16   field_74; // copied from Display_State.field_10e (func_800AAA68); low byte is CdCmd 0x21 param2[0]
    s16   field_76; // set: func_800AD378 rebuilds via func_800ACD2C
    s16   field_78; // cached GameSession.field_7; func_800AADDC
    byte  unknown_7A[2];
    s16   field_7C;
    s16   field_7E;
    s16   field_80;
    byte  unknown_82[0x9A];
    s16   field_11C; // cached Mc_SaveData.field_22; -1 forces refresh (func_800B7D18 / func_800AABB0)
    s16   field_11E; // cached Wip_SysConfig.field_26; refreshed with field_11C
    byte  unknown_120[4];
    u8    field_124; // companion type 1/2/3; written from func_800ABA4C's return
    u8    field_125; // written with Mc_SaveData.field_5C7 (func_800A9CBC)
    u8    field_126;
    u8    field_127; // 0: run death / companion-down checks in func_800A0094
    u8    field_128; // 0xFF sentinel in func_800B0748 / func_800B082C
    u8    field_129; // last CdCmd 0x21 param[0] written by func_800B065C
    byte  unknown_12A[2];
    u8    field_12C;
    s8    field_12D; // lb/sb countdown; 0x7F sentinel in func_800A7320
    u8    field_12E; // copied as s8 into D_80114BD8.field_2 (func_800A76A4)
    u8    field_12F;
    byte  unknown_130[9];
    u8    field_139;
    u8    field_13A; // cleared by func_800AE45C when D_80114CDC is 0
    u8    field_13B;
} GameSession;
STATIC_ASSERT_SIZEOF(GameSession, 0x13C);

/// Overlay of `GameSession` starting at offset 0x4 (`field_4`..`field_9`).
/// Used when the compiler keeps `&Game_Session->field_4` in a register.
/// `func_800DB128` packs `field_3` / `field_2` / `field_5` into a location key.
typedef struct _GameSessionFrom4 {
    /* 0x0 */ u8 field_0; // GameSession.field_4
    /* 0x1 */ u8 field_1; // GameSession.field_5
    /* 0x2 */ u8 field_2; // GameSession.field_6
    /* 0x3 */ u8 field_3; // GameSession.field_7
    /* 0x4 */ u8 field_4; // GameSession.field_8
    /* 0x5 */ u8 field_5; // GameSession.field_9
} GameSessionFrom4;
STATIC_ASSERT_SIZEOF(GameSessionFrom4, 0x6);

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
/// (bits 0/1: `func_80101848` case 8; bit 0x100: `func_8010583C`;
/// bits 0x102: `func_80105894`). Count is
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
    /* 0x000 */ byte                pad_0[0x10];
    /* 0x010 */ s32                 field_10; // copy of GsCOORDINATE2.coord.t[0]
    /* 0x014 */ s32                 field_14; // copy of GsCOORDINATE2.coord.t[1]
    /* 0x018 */ s32                 field_18; // copy of GsCOORDINATE2.coord.t[2]
    /* 0x01C */ byte                pad_1C[4];
    /* 0x020 */ s32                 field_20; // copied from func_80105070 arg2
    /* 0x024 */ s32                 field_24;
    /* 0x028 */ s32                 field_28;
    /* 0x02C */ byte                pad_2C[0x14];
    /* 0x040 */ s32                 field_40;
    /* 0x044 */ s32                 field_44;
    /* 0x048 */ s32                 field_48;
    /* 0x04C */ byte                pad_4C[4];
    /* 0x050 */ s16                 field_50; // SVECTOR.vx; func_80104D68 / RotMatrix
    /* 0x052 */ s16                 field_52; // facing angle (lh); func_8010BCF4 / func_80103E7C
    /* 0x054 */ s16                 field_54; // SVECTOR.vz; func_80104D68 / RotMatrix
    /* 0x056 */ byte                pad_56[2];
    /* 0x058 */ s16                 field_58;
    /* 0x05A */ byte                pad_5A[2];
    /* 0x05C */ s16                 field_5C;
    /* 0x05E */ byte                pad_5E[2];
    /* 0x060 */ s16                 field_60;
    /* 0x062 */ byte                pad_62[2];
    /* 0x064 */ s16                 field_64;
    /* 0x066 */ byte                pad_66[2];
    /* 0x068 */ s16                 field_68;
    /* 0x06A */ s16                 field_6A;       // aim/look yaw offset; func_8010BE5C
    /* 0x06C */ byte                pad_6C[4];
    /* 0x070 */ s16                 field_70;       // pitch-like angle; func_80102D20
    /* 0x072 */ byte                pad_72[6];
    /* 0x078 */ s16                 field_78;       // pitch; func_80102F10
    /* 0x07A */ byte                pad_7A[6];
    /* 0x080 */ s16                 field_80;       // copied from func_80104F5C arg2
    /* 0x082 */ s16                 field_82;       // target facing angle; func_80104E00 / func_80108BD8
    /* 0x084 */ byte                pad_84[4];
    /* 0x088 */ byte                field_88[8];    // address taken as GpObj.field_C
    /* 0x090 */ s32                 field_90;
    /* 0x094 */ byte                field_94[8];    // address taken as GpObj.field_C
    /* 0x09C */ s32                 field_9C;
    /* 0x0A0 */ byte                field_A0[8];    // address taken as GpObj.field_C
    /* 0x0A8 */ s32                 field_A8;
    /* 0x0AC */ byte                field_AC[0x20]; // 0x20-byte list node (func_800E1638)
    /* 0x0CC */ byte                field_CC[0x20];
    /* 0x0EC */ byte                field_EC[0x20];
    /* 0x10C */ byte                field_10C[0x18]; // 0x20-byte list node; field_124 is +0x18
    /* 0x124 */ u32                 field_124;
    /* 0x128 */ byte                pad_128[2];
    /* 0x12A */ u16                 field_12A;
    /* 0x12C */ byte                field_12C[0x20];
    /* 0x14C */ byte                field_14C[0x18]; // GpActorD4Rec; func_80100FCC
    /* 0x164 */ byte                pad_164[0x18];
    /* 0x17C */ GpRec18             field_17C[18];   // func_800E1A6C / func_801041B4
    /* 0x32C */ GpRec18             field_32C[6];    // func_80100FCC / func_800E18E0
    /* 0x3BC */ byte                pad_3BC[0x18];
    /* 0x3D4 */ byte                field_3D4[0x50]; // GsCOORDINATE2; func_80100FCC
    /* 0x424 */ byte                field_424[0x14]; // GpAnimCtx overlay; func_800B4514
    /* 0x438 */ byte                pad_438[9];      // GpAnimSlot array base; func_80105B0C
    /* 0x441 */ u8                  field_441;       // slid-actor overlay; see GameActorSlot
    /* 0x442 */ byte                pad_442[6];
    /* 0x448 */ GameActorSlot       field_448[19];
    /* 0x740 */ byte                pad_740[0x68];
    /* 0x7A8 */ byte                field_7A8; // addr taken as func_800B3F84 arg3
    /* 0x7A9 */ byte                pad_7A9[0x163];
    /* 0x90C */ struct _GpLinkNode* field_90C;
    /* 0x910 */ struct _GpActorD4*  field_910;
    /* 0x914 */ struct _Task*       field_914;
    /* 0x918 */ struct _Task*       field_918;
    /* 0x91C */ struct _Task*       field_91C;
    /* 0x920 */ struct _Task*       field_920;
    /* 0x924 */ struct _Task*       field_924;
    /* 0x928 */ void*               field_928; // D_80112D6C[field_93A]; func_800B3F84 arg1
    /* 0x92C */ byte                pad_92C[4];
    /* 0x930 */ s32                 field_930; // sw from func_800AE1F0; addr taken by func_801011D0
    /* 0x934 */ s32                 field_934;
    /* 0x938 */ s16                 field_938; // GameActorSlot count (init 0x13)
    /* 0x93A */ u16                 field_93A; // D_80112D68[field_22-1] + field_21
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
    /* 0x964 */ u16                 field_964; // previous field_962
    /* 0x966 */ u16                 field_966;
    /* 0x968 */ u16                 field_968; // released buttons: field_964 & ~field_962
    /* 0x96A */ u16                 field_96A; // set to 0xF89A by func_8010615C
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
    /* 0x97B */ s8                  field_97B;
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
    /* 0x986 */ u8                  field_986;
    /* 0x987 */ u8                  field_987; // texture upload seq A (func_801030CC / D_80112E74)
    /* 0x988 */ u8                  field_988; // field_987 delay; reload 4 after each upload
    /* 0x989 */ u8                  field_989; // field_987 frame index
    /* 0x98A */ u8                  field_98A; // texture upload seq B (func_801030CC / D_80112EB4)
    /* 0x98B */ u8                  field_98B; // field_98A delay; reload 8 after each upload
    /* 0x98C */ u8                  field_98C; // field_98A frame index
    /* 0x98D */ u8                  field_98D;
    /* 0x98E */ u8                  field_98E;
    /* 0x98F */ u8                  field_98F; // cleared by func_801034C0
    /* 0x990 */ u8                  field_990;
    /* 0x991 */ s8                  field_991; // func_80109374 requires 0 to write field_97D = 1
    /* 0x992 */ u8                  field_992; // func_80100E40: func_801011D0 result when field_984 & 1
    /* 0x993 */ u8                  field_993;
} GameActor;
STATIC_ASSERT_SIZEOF(GameActor, 0x994);

/// Object pointed to by Task::extra; field_8 is a GsCOORDINATE2* (stored as
/// s32* so Display_SpawnFromMode can clear flg via *ptr = 0) after optional
/// func_801011D0 / func_800E1A6C setup. field_C flag bits are OR'd with 0x80
/// in Task_Kill (type-1 deferred kill). `func_800BBC10` writes 8 on first run
/// and clears bit 0x8 before Task_CallExit. A non-NULL `field_18` lets
/// `func_800D8EA0` rebuild the color matrix even when
/// `Game_Session->field_65 == 1` (unless bit 0x80 of `field_C` is set).
/// field_1C / field_20 are MATRIX* defaults (`D_80114E98` / `D_80114EB8`)
/// written by `func_800D9D18`.
typedef struct _GameActorExt {
    /* 0x00 */ byte  pad_0[0x8];
    /* 0x08 */ s32*  field_8;
    /* 0x0C */ u16   field_C;
    /* 0x0E */ byte  pad_E[0xA];
    /* 0x18 */ void* field_18;
    /* 0x1C */ void* field_1C;
    /* 0x20 */ void* field_20;
} GameActorExt;
STATIC_ASSERT_SIZEOF(GameActorExt, 0x24);

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
