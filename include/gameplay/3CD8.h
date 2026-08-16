#ifndef GAMEPLAY_3CD8_H
#define GAMEPLAY_3CD8_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/task.h"

/// 0xC-byte sequence-table record. `D_801155A8` is the current table
/// (`func_800E41F4` stores its first arg there). `func_800E6EA0` walks
/// from a start index until `field_8 == -1` (terminator) or `field_5`
/// equals `D_80115668` (the key `func_800E41F4` saved from its third arg).
typedef struct _GpEvt12 {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ u8  field_2;
    /* 0x3 */ u8  field_3;
    /* 0x4 */ u8  field_4; // flags copied to D_80115670; bit 0 cleared if field_7
    /* 0x5 */ u8  field_5; // compared with D_80115668
    /* 0x6 */ u8  field_6;
    /* 0x7 */ u8  field_7; // copied to D_80115678
    /* 0x8 */ s32 field_8; // -1 terminator, else payload/id
} GpEvt12;
STATIC_ASSERT_SIZEOF(GpEvt12, 0xC);

/// Three overlay ids passed through `func_800E7498` (table cmd 0xFA6)
/// to `CdCmd_StartOverlay`.
typedef struct _GpOverlayIds {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
    /* 0x4 */ u16 field_4;
} GpOverlayIds;
STATIC_ASSERT_SIZEOF(GpOverlayIds, 6);

/// Packed bytes in `Task::spawnArg1` for `func_800E6F60`.
/// `field_0` is forwarded as a2 to `func_800AC464`.
/// `field_1` is copied into `Task::killCountdown` on state 0.
/// `field_2` selects the target: 0 = slot 3, 1 = slot 0xA, else
/// `func_800E86FC(field_2 - 2)`.
typedef struct _GpSpawnArg {
    /* 0x0 */ u8 field_0;
    /* 0x1 */ u8 field_1;
    /* 0x2 */ u8 field_2;
    /* 0x3 */ u8 field_3;
} GpSpawnArg;
STATIC_ASSERT_SIZEOF(GpSpawnArg, 4);

/// Object stored in `Task::spawnArg2` for `func_800E712C`. `field_2` is a
/// signed completion flag: when non-zero the task calls `Stage_SetEndingFlag`
/// and kills itself.
typedef struct _GpEndWait {
    /* 0x00 */ byte pad_0[2];
    /* 0x02 */ s8   field_2;
} GpEndWait;

/// 0x34-byte dual-script state allocated by `func_800E8758` (`Mem_Calloc(0x34, 0)`)
/// and stored on the owner task at +0x1C (`Task::idMap`).
/// `field_10` / `field_11` are delay counters for scripts A / B.
typedef struct _GpState34 {
    /* 0x00 */ void* field_0;  // script table (from Task::spawnArg2)
    /* 0x04 */ void* field_4;  // secondary table
    /* 0x08 */ s16   field_8;
    /* 0x0A */ u16   field_A;  // current command (script A)
    /* 0x0C */ u16   field_C;  // current command (script B)
    /* 0x0E */ u8    field_E;  // script A PC
    /* 0x0F */ u8    field_F;  // script B PC
    /* 0x10 */ u8    field_10; // delay A
    /* 0x11 */ u8    field_11; // delay B
    /* 0x12 */ u8    field_12;
    /* 0x13 */ u8    field_13;
    /* 0x14 */ u8    field_14; // loop counter A
    /* 0x15 */ u8    field_15; // loop counter B
    /* 0x16 */ byte  pad_16[0x12];
    /* 0x28 */ s32   field_28;
    /* 0x2C */ s32   field_2C;
    /* 0x30 */ s32   field_30;
} GpState34;
STATIC_ASSERT_SIZEOF(GpState34, 0x34);

/// 0x18-byte work block allocated by `func_800E8FB0` / `func_800E9188`
/// (`Mem_Calloc(0x18)`) and stored at `Task::idMap` for bank-2 type 0xD.
/// `func_800E8FB0` writes `field_0`/`field_4` from its args and clears
/// `field_8`; `func_800E9188` writes all three. `func_800E9070` indexes
/// dispatch tables with `field_A` and `field_C`.
typedef struct _GpState18 {
    /* 0x00 */ s32  field_0;
    /* 0x04 */ s32  field_4;
    /* 0x08 */ s16  field_8;
    /* 0x0A */ u8   field_A; // dispatch index A (func_800E9070)
    /* 0x0B */ u8   field_B;
    /* 0x0C */ u8   field_C; // dispatch index B (func_800E9070)
    /* 0x0D */ byte pad_D[0xB];
} GpState18;
STATIC_ASSERT_SIZEOF(GpState18, 0x18);

/// 0x1C-byte halfword state allocated by `func_800E9CC8` (`func_8002D978(0x1C)`)
/// and stored in `D_80115740` (also written to the owner task at +0x1C).
typedef struct _GpState1C {
    /* 0x00 */ s16 field_0;
    /* 0x02 */ s16 field_2;
    /* 0x04 */ s16 field_4;
    /* 0x06 */ s16 field_6;
    /* 0x08 */ s16 field_8;
    /* 0x0A */ s16 field_A;
    /* 0x0C */ s16 field_C;
    /* 0x0E */ s16 field_E;
    /* 0x10 */ s16 field_10; // flags (bit 0 checked by func_800EC9C8, bit 0x80 by func_800ECA54)
    /* 0x12 */ s16 field_12; // flags (bit 0x800 cleared by func_800ECA54)
    /* 0x14 */ s16 field_14;
    /* 0x16 */ s16 field_16;
    /* 0x18 */ s16 field_18; // PE bit written by func_800ECA10
    /* 0x1A */ u16 field_1A; // flags (0x80 by func_800FC6C0, 0x100 by func_800EC868)
} GpState1C;
STATIC_ASSERT_SIZEOF(GpState1C, 0x1C);

/// 0x64-byte world-coord slot. `func_800EA3EC` inits all 8 entries of
/// `D_80114F30`: `coord.sub` is the parent (`&D_80070F10`) and `field_0`
/// is a refcount (decremented by `func_800EA3B4`). `func_800D9618` returns
/// how many slots currently have a non-zero refcount.
typedef struct _GpCoord64 {
    /* 0x00 */ s32           field_0;
    /* 0x04 */ GsCOORDINATE2 coord;
    /* 0x54 */ byte          pad_54[0x10];
} GpCoord64;
STATIC_ASSERT_SIZEOF(GpCoord64, 0x64);

extern GpState1C*    D_80115740;
extern GpCoord64     D_80114F30[8];
extern GsCOORDINATE2 D_80070F10;

void func_800E34D8(s32 arg0, s16 arg1);
void func_800E3B80(s32 arg0);
void func_800E3BBC(s32 arg0);
void func_800E4080(void);
void func_800E40BC(s32 arg0, s32 arg1);
s32  func_800E6CE0(void);

void func_800E9C6C(void);
s16  func_800EA1A8(VECTOR3* arg0, VECTOR3* arg1);
s32  func_800EA318(s16 arg0, s16 arg1, s16 arg2);
void func_800EA3A0(s32 arg0);
void func_800EA3B4(void);
void func_800EA3EC(void);
void func_800EA478(s32 arg0, GsCOORDINATE2* arg1, s32 arg2, s32 arg3);
void func_800EC7E4(void* arg0, Task* arg1);
void func_800EC868(void);
void func_800EC9C8(void);
void func_800ECA10(s32 arg0);
void func_800ECA54(void);

#endif // GAMEPLAY_3CD8_H
