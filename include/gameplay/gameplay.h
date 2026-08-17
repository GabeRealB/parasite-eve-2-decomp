#ifndef GAMEPLAY_GAMEPLAY_H
#define GAMEPLAY_GAMEPLAY_H

#include "common.h"

#include "main/session.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/tmd.h"

/// Global at `D_80114C08`. `field_0` is a u16 loaded by many helpers.
/// `field_3` is a signed state byte (`lb`); `func_80109290` compares it to -2
/// and `func_80109374` requires 0. `field_6` is a flags byte (bit 0 gates
/// `func_800A7DB8` writing `field_E`; bit 1 is cleared by `func_800A7574`
/// and forces `func_800A7E5C` to 0 when that function's arg is 0).
/// `field_A` is a signed byte (`lb`); `func_800A7DE0` sets `field_3 = 2`
/// when it is >= 2, then clears it. `func_800A7574` also zeros `field_A`,
/// `field_C`..`field_F`, `field_10`/`field_12`/`field_14`, and
/// `field_16`/`field_17`. Those two bytes are also the item 4 / item 8
/// gates in `func_800D6170` (`lb`).
typedef struct _GpStateC08 {
    /* 0x00 */ u16  field_0;
    /* 0x02 */ byte pad_2;
    /* 0x03 */ s8   field_3;
    /* 0x04 */ byte pad_4[2];
    /* 0x06 */ u8   field_6;
    /* 0x07 */ u8   field_7;
    /* 0x08 */ u8   field_8;
    /* 0x09 */ byte pad_9;
    /* 0x0A */ s8   field_A;
    /* 0x0B */ byte pad_B;
    /* 0x0C */ s8   field_C;
    /* 0x0D */ u8   field_D;
    /* 0x0E */ u8   field_E;
    /* 0x0F */ u8   field_F;
    /* 0x10 */ s16  field_10;
    /* 0x12 */ s16  field_12;
    /* 0x14 */ s16  field_14;
    /* 0x16 */ s8   field_16;
    /* 0x17 */ u8   field_17;
} GpStateC08;
STATIC_ASSERT_SIZEOF(GpStateC08, 0x18);

extern GpStateC08 D_80114C08;

/// Pair of s32 working copies at `D_80114BE8`. `func_800A7574` (and
/// `func_800B996C`) sign-extend `Wip_SysConfig.field_18` / `field_1c` into
/// `field_0` / `field_4`. Splat also emits `D_80114BEC` at +4.
typedef struct _GpStateBE8 {
    /* 0x0 */ s32 field_0;
    /* 0x4 */ s32 field_4;
} GpStateBE8;
STATIC_ASSERT_SIZEOF(GpStateBE8, 0x8);

extern GpStateBE8 D_80114BE8;

/// +0xC overlay of the 0x30-byte record `func_8009FEDC` allocates with
/// `Mem_Calloc(0x30, 0)` and stores at `Task::idMap`. `func_800A7574` is
/// called with that pointer + 0xC; it writes `field_16 = -1` and clears
/// `field_18`.
typedef struct _GpIdMapC {
    /* 0x00 */ byte pad_0[0x16];
    /* 0x16 */ s8   field_16;
    /* 0x17 */ byte pad_17;
    /* 0x18 */ s16  field_18;
} GpIdMapC;
STATIC_ASSERT_SIZEOF(GpIdMapC, 0x1A);

/// 4-byte spawn arg at `D_80114BD8`. `func_800A76A4` zeros `field_0` / `field_1`,
/// writes `GameSession.field_12E` as a signed halfword to `field_2`, then passes
/// the record to `Task_Spawn(1, 0x31, 0, ...)`.
typedef struct _GpStateBD8 {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ s16 field_2;
} GpStateBD8;
STATIC_ASSERT_SIZEOF(GpStateBD8, 0x4);

extern GpStateBD8 D_80114BD8;

/// 0x24-byte camera/view record in tables pointed to by `D_8010CB2C`.
/// Indexed 1-based by `func_800AD284()`. `mtx` is copied to `D_80070E44` /
/// `D_80070F28` by `func_800A8724`; `field_20` is loaded as both `lhu` and `lw`.
typedef struct _GpCb2CRec {
    /* 0x00 */ MATRIX mtx;
    /* 0x20 */ u32    field_20;
} GpCb2CRec;
STATIC_ASSERT_SIZEOF(GpCb2CRec, 0x24);

/// Per-stage wrapper. `field_0` is an array of `GpCb2CRec*`, indexed by
/// `GameSession.field_6 - 1` / `GameSessionFrom4.field_2 - 1`.
typedef struct _GpCb2CTbl {
    /* 0x0 */ GpCb2CRec** field_0;
} GpCb2CTbl;

/// Per-stage pointer table. Index is `GameSession.field_7 - 1`.
extern GpCb2CTbl* D_8010CB2C[];

/// `GsCOORDINATE2` overlay embedded in `GpDisp2d` at +0x10. Same 0x50 layout
/// as libgs, but offset 0x44 (`param` / first half of `super`) is an `SVECTOR`
/// of zeros written by `func_80099098`. `sub` is still the parent coordinate
/// (`&D_80070F10`).
typedef struct _GpDisp2dCoord {
    /* 0x00 */ u32            flg;
    /* 0x04 */ MATRIX         mtx;
    /* 0x24 */ MATRIX         workm;
    /* 0x44 */ SVECTOR        rot;
    /* 0x4C */ GsCOORDINATE2* sub;
} GpDisp2dCoord;
STATIC_ASSERT_SIZEOF(GpDisp2dCoord, 0x50);

/// 0x60-byte spawnType-2 extra (`Mem_Calloc` in `func_80099098`, fail string
/// `"new_disp_2d ----> NULL"`). Linked onto `Tmd_ListAlt`. `field_8` points at
/// the embedded coord; `field_C` is stored as a word 1.
typedef struct _GpDisp2d {
    /* 0x00 */ TmdObject*     next;
    /* 0x04 */ TmdListHead*   prev;
    /* 0x08 */ GpDisp2dCoord* field_8;
    /* 0x0C */ s32            field_C;
    /* 0x10 */ GpDisp2dCoord  coord;
} GpDisp2d;
STATIC_ASSERT_SIZEOF(GpDisp2d, 0x60);

void func_80098F58(GsCOORDINATE2* arg0);
void func_80098F98(GsCOORDINATE2* arg0, s32 arg1);
Task* func_8009988C(GsCOORDINATE2* arg0);
u8*  func_800A746C(void);
s32  func_800A74C4(void);
void func_800A7574(GpIdMapC* arg0);
s32  func_800A7B20(s32 arg0);
void func_800A784C(void* arg0);
void func_800A7A64(void);
void func_800A7DB8(s32 arg0);
void func_800A7DE0(void);
s32  func_800A7E5C(s32 arg0);
void func_800A8B14(void);
void func_800A8B6C(void);
GpCb2CRec* func_800A8C08(GameSessionFrom4* arg0);
void func_800A8D5C(void);
void func_800A8DC0(s32 arg0);
void func_800A9010(Task* task);
void func_800A91CC(Task* task);

#endif // GAMEPLAY_GAMEPLAY_H
