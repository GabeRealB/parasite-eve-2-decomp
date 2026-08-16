#ifndef GAMEPLAY_1A8_H
#define GAMEPLAY_1A8_H

#include "common.h"

#include "main/task.h"

/// Overlay of `GsCOORDINATE2` at `GameActorExt.field_8`. `field_18` /
/// `field_20` are the low 16 bits of `coord.t[0]` / `coord.t[2]` (world X/Z).
typedef struct _GpCoordXZ {
    /* 0x00 */ byte pad_0[0x18];
    /* 0x18 */ u16  field_18;
    /* 0x1A */ byte pad_1A[6];
    /* 0x20 */ u16  field_20;
} GpCoordXZ;
STATIC_ASSERT_SIZEOF(GpCoordXZ, 0x22);

/// SVECTOR layout with unsigned X/Z so `func_800AEE28` emits `lhu`.
typedef struct _GpPosXZ {
    /* 0x0 */ u16 vx;
    /* 0x2 */ u16 pad_2;
    /* 0x4 */ u16 vz;
    /* 0x6 */ u16 pad_6;
} GpPosXZ;
STATIC_ASSERT_SIZEOF(GpPosXZ, 8);

/// 2-byte record in tables pointed to by `D_8010CB40`. Indexed by
/// `GameSession.field_5 - 1`. `func_800AEEFC` loads `field_0`; `func_800ACEBC`
/// loads the same cell as a signed halfword.
typedef struct _GpCb40Rec {
    /* 0x0 */ u8 field_0;
    /* 0x1 */ u8 field_1;
} GpCb40Rec;
STATIC_ASSERT_SIZEOF(GpCb40Rec, 2);

/// Per-stage wrapper. `field_0` is an array of `GpCb40Rec*`, indexed by
/// `GameSession.field_6 - 1`.
typedef struct _GpCb40Tbl {
    /* 0x0 */ GpCb40Rec** field_0;
} GpCb40Tbl;

/// Per-stage pointer table. Index is `GameSession.field_7 - 1`.
extern GpCb40Tbl* D_8010CB40[];

/// 8-byte dest-location payload at `D_80114CE8`. `func_800AF284` copies
/// `field_0` / `field_2` / `field_3` into `Mc_SaveData.field_6` /
/// `field_8` / `field_5` before `Task_Spawn(0, 0x11, ...)`. Nearby D4
/// helpers also write `field_4` / `field_5` / `field_6`.
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

extern GpSaveLoc D_80114CE8;

/// 2-byte record in 0xFF-terminated lists walked by `func_800AF500`.
/// `field_0` indexes `D_8010CBCC[arg0]` (same role as `GpAreaKey.field_2`);
/// `field_1` is the apply flag (nonzero → `GpAreaObj.field_1 |= 4`).
typedef struct _GpAreaFlagRec {
    /* 0x0 */ u8 field_0;
    /* 0x1 */ u8 field_1;
} GpAreaFlagRec;
STATIC_ASSERT_SIZEOF(GpAreaFlagRec, 2);

/// 6-entry table of no-arg callbacks. Copied onto the stack by
/// `func_800AEF4C` (`D_80093978`) and the sibling dispatcher
/// `func_800AEFBC` (`D_80093990`).
typedef void (*GpVoidFunc)(void);

typedef struct {
    GpVoidFunc funcs[6];
} GpVoidFuncTable6;

s32  func_800AEE28(Task* arg0, GpPosXZ* arg1);
u8   func_800AEEFC(void);
void func_800AF500(s16 arg0, GpAreaFlagRec* arg1);

#endif // GAMEPLAY_1A8_H
