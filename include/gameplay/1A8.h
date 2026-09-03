#ifndef GAMEPLAY_1A8_H
#define GAMEPLAY_1A8_H

#include "common.h"

#include "main/task.h"

/// Overlay of `GsCOORDINATE2` at `TmdObject.field_8`. `field_18` /
/// `field_20` are the low 16 bits of `coord.t[0]` / `coord.t[2]` (world X/Z).
typedef struct _GpCoordXZ {
    /* 0x00 */ byte pad_0[0x18];
    /* 0x18 */ u16  field_18;
    /* 0x1A */ byte pad_1A[6];
    /* 0x20 */ u16  field_20;
} GpCoordXZ;
STATIC_ASSERT_SIZEOF(GpCoordXZ, 0x22);

/// SVECTOR layout with unsigned X/Z so `Gp_YawToPosXZ` emits `lhu`.
typedef struct _GpPosXZ {
    /* 0x0 */ u16 vx;
    /* 0x2 */ u16 pad_2;
    /* 0x4 */ u16 vz;
    /* 0x6 */ u16 pad_6;
} GpPosXZ;
STATIC_ASSERT_SIZEOF(GpPosXZ, 8);

/// 2-byte record in tables pointed to by `Gp_ViewCountTables`. Indexed by
/// `GameSession.field_5 - 1`. `Gp_GetViewCountLo` loads `field_0`; `Gp_FindViewIndex`
/// loads the same cell as a signed halfword.
typedef struct _GpViewCountRec {
    /* 0x0 */ u8 field_0;
    /* 0x1 */ u8 field_1;
} GpViewCountRec;
STATIC_ASSERT_SIZEOF(GpViewCountRec, 2);

/// Per-stage wrapper. `field_0` is an array of `GpViewCountRec*`, indexed by
/// `GameSession.field_6 - 1`.
typedef struct _GpViewCountTbl {
    /* 0x0 */ GpViewCountRec** field_0;
} GpViewCountTbl;

/// Per-stage pointer table. Index is `GameSession.field_7 - 1`.
extern GpViewCountTbl* Gp_ViewCountTables[];

/// 8-byte dest-location payload at `Gp_WarpLoc`. `Gp_CommitDirWarp` fills it
/// (halfword `field_0`/`field_1` from `Gp_DirAlt`, `field_2` from
/// `Gp_DirAltNibble & 0xF`, `field_3`/`field_4` = 1, `field_5` = 0), posts slot-7
/// msg `0x13EE`, then copies `field_0` / `field_2` / `field_3` into
/// `Mc_SaveData.field_6` / `field_8` / `field_5` before `Task_Spawn(0, 0x11,
/// ...)`. `Gp_CommitWarp` fills the same payload from `Gp_DirByte` /
/// `Gp_DirNibble & 0xF` and `GpWarpRec.field_36`. `Gp_CommitSaveLoc` does the
/// same copy + spawn.
typedef struct _GpSaveLoc {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ u8  field_2;
    /* 0x3 */ u8  field_3;
    /* 0x4 */ u8  field_4;
    /* 0x5 */ u8  field_5;
    /* 0x6 */ u16 field_6;
} GpSaveLoc;
STATIC_ASSERT_SIZEOF(GpSaveLoc, 8);

extern GpSaveLoc Gp_WarpLoc;

/// Dual bitmask of area ids 1–32 / 33–64. Zeroed by `Gp_InitDirState`.
/// `Gp_RebuildAreaIdBits` sets a bit when `GpAreaObj.field_1` bit 2 is set and
/// `Gp_GetAreaFlag2` is 0, otherwise clears it.
extern s32 Gp_AreaIdBits[2];

/// 2-byte record in 0xFF-terminated lists walked by `Gp_ApplyAreaFlag4List` and
/// `Gp_ApplyNewGameAreaFlags`. `field_0` indexes a `GpAreaRec` table (same role as
/// `GpAreaKey.field_2`); `field_1` is the apply flag (nonzero →
/// `GpAreaObj.field_1 |= 4`).
typedef struct _GpAreaFlagRec {
    /* 0x0 */ u8 field_0;
    /* 0x1 */ u8 field_1;
} GpAreaFlagRec;
STATIC_ASSERT_SIZEOF(GpAreaFlagRec, 2);

/// 4-byte stack payload for slot-7 msg `0x13EF`. `Gp_PostMsg13EF` copies
/// `Gp_DirFlags` / `Gp_DirByte` / `Gp_DirNibble` into the three fields.
typedef struct _GpMsg13EF {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u8  field_2;
    /* 0x3 */ u8  field_3;
} GpMsg13EF;
STATIC_ASSERT_SIZEOF(GpMsg13EF, 4);

/// 0x18-byte stack payload for the slot-3 warp messages. `Gp_MsgPlayer3EE`
/// writes `Gp_DirNibble << 4` at `field_12` and zeros `field_10` / `field_14`;
/// senders of msg `0x3F2` fill the destination position in
/// `field_0` / `field_4` / `field_8` instead.
typedef struct _GpMsg3EE {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
    /* 0x10 */ s16 field_10;
    /* 0x12 */ s16 field_12;
    /* 0x14 */ s16 field_14;
    /* 0x16 */ s16 pad_16;
} GpMsg3EE;
STATIC_ASSERT_SIZEOF(GpMsg3EE, 0x18);

/// Tables of no-arg callbacks copied onto the stack by the sibling
/// dispatchers. `Gp_DirAction0` copies the 6-entry `Gp_WarpPhaseFns`;
/// `Gp_DirAction1` copies the 5-entry `D_80093990`.
typedef void (*GpVoidFunc)(void);

typedef struct {
    GpVoidFunc funcs[5];
} GpVoidFuncTable5;

typedef struct {
    GpVoidFunc funcs[6];
} GpVoidFuncTable6;

/// Per-stage flag-nibble lookup. `arg0` indexes a u16 table selected by
/// `Game_Session->field_7` (1..5). Low 11 bits are the `GameFlag_GetNibble`
/// index; bit `0x800` is added onto the result. Unknown stage or out-of-range
/// index returns -1.
s16  Gp_LookupStageFlag(s32 arg0);
s32  Gp_YawToPosXZ(Task* arg0, GpPosXZ* arg1);
u8   Gp_GetViewCountLo(void);
void Gp_SetCurAreaFlag4(void);
void Gp_ApplyAreaFlag4List(s16 arg0, GpAreaFlagRec* arg1);

#endif // GAMEPLAY_1A8_H
