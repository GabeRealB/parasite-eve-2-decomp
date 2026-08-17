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
/// When not -1, `field_8` is a relocated `u16*` text stream walked by
/// `func_800E6AD4` / `func_800E69F4` / `func_800E67C8` (codes `-1` end,
/// `-2` newline, `-3` skip; else glyph index `& 0x3FF` into `D_8011567C`).
typedef struct _GpEvt12 {
    /* 0x0 */ u8  field_0;
    /* 0x1 */ u8  field_1;
    /* 0x2 */ u8  field_2;
    /* 0x3 */ u8  field_3;
    /* 0x4 */ u8  field_4; // flags copied to D_80115670; bit 0 cleared if field_7
    /* 0x5 */ u8  field_5; // compared with D_80115668
    /* 0x6 */ u8  field_6;
    /* 0x7 */ u8  field_7; // copied to D_80115678
    /* 0x8 */ s32 field_8; // -1 terminator, else relocated u16* text
} GpEvt12;
STATIC_ASSERT_SIZEOF(GpEvt12, 0xC);

/// Three overlay ids passed through `func_800E7498` (table cmd 0xFA6)
/// to `CdCmd_StartOverlay`. Also the current evs triple held in
/// `D_801156F4` (`func_800E646C` formats `"evs%d_%d_%d.txt"` from it).
typedef struct _GpOverlayIds {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
    /* 0x4 */ u16 field_4;
} GpOverlayIds;
STATIC_ASSERT_SIZEOF(GpOverlayIds, 6);

/// 0x14-byte payload copied from `D_8010FB38` by `func_800E3BBC` (arg 0)
/// and sent as slot-3 msg `0x3E8`. `field_0` is overwritten with
/// `D_80112D68[Mc_SaveData.field_22 - 1] + Wip_SysConfig.field_21`
/// (same value `func_800E3CEC` writes). `func_800E3EF0` copies the same
/// record to slot 0xA and overwrites `field_0` with
/// `D_80113360[Mc_SaveData.field_13 - 1] + Mc_SaveData.field_5C7`
/// (same value `func_800E3D24` writes). Nearby `D_8010FB10` /
/// `D_8010FB24` are the same size.
typedef struct _GpRec14 {
    /* 0x00 */ s32 field_0;
    /* 0x04 */ s32 field_4;
    /* 0x08 */ s32 field_8;
    /* 0x0C */ s32 field_C;
    /* 0x10 */ s32 field_10;
} GpRec14;
STATIC_ASSERT_SIZEOF(GpRec14, 0x14);

/// 4-byte volume-fade payload at `Task::spawnArg2` for `func_800E8378`.
/// `field_0` is the target volume passed to `Snd_ApplyVolumeTable`.
/// `field_2` is the fade duration in frames (`0` applies immediately).
typedef struct _GpVolFade {
    /* 0x0 */ u16 field_0; // target volume
    /* 0x2 */ u16 field_2; // duration
} GpVolFade;
STATIC_ASSERT_SIZEOF(GpVolFade, 4);

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

/// 4-byte dual-script command. `GpState34::field_0` is an array of these.
/// Script A reads `field_0`, script B reads `field_2`. Low byte is the opcode
/// (0 = stop, 1 = timed pad from `field_4`, 2 = set delay, 3 = set/decrement
/// loop, 4 = loop jump); high byte is the payload.
typedef struct _GpScriptCmd {
    /* 0x0 */ u16 field_0; // script A command
    /* 0x2 */ u16 field_2; // script B command
} GpScriptCmd;
STATIC_ASSERT_SIZEOF(GpScriptCmd, 4);

/// 4-byte pad record. `GpState34::field_4` is an array of these, indexed by
/// the high byte of an opcode-1 command. `field_2` is the delay copied to
/// `field_10` / `field_11`; `field_0` / `field_1` are start/end for script B.
typedef struct _GpScriptRec {
    /* 0x0 */ u8 field_0;
    /* 0x1 */ u8 field_1;
    /* 0x2 */ u8 field_2; // delay
    /* 0x3 */ u8 field_3;
} GpScriptRec;
STATIC_ASSERT_SIZEOF(GpScriptRec, 4);

/// 0x34-byte dual-script state allocated by `func_800E8758` (`Mem_Calloc(0x34, 0)`)
/// and stored on the owner task at +0x1C (`Task::idMap`).
/// `field_10` / `field_11` are delay counters for scripts A / B.
typedef struct _GpState34 {
    /* 0x00 */ GpScriptCmd* field_0; // script table (from Task::spawnArg2)
    /* 0x04 */ GpScriptRec* field_4; // secondary pad table
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

/// 0xC-byte interpolator state allocated by `func_800E8D1C` / `func_800E8E00`
/// (`Mem_Calloc(0xC, 0)`) and stored at `Task::idMap` for bank-2 type 0xC.
/// `field_8` is the duration; `field_4.as_s32` is start<<8; `field_0` is the
/// per-frame step `((end<<8) - (start<<8)) / duration`.
/// `func_800E9498` posts `field_4.bytes.as_u8` (the 8-bit interpolator,
/// `as_s32 >> 8` on little-endian) via `Pad_PostEvent`.
typedef struct _GpState0C {
    /* 0x0 */ s32 field_0; // step
    /* 0x4 */ union {
        s32 as_s32; // start << 8
        struct {
            /* 0x4 */ u8 pad_4;
            /* 0x5 */ u8 as_u8; // (as_s32 >> 8)
        } bytes;
    } field_4;
    /* 0x8 */ s16  field_8; // duration
    /* 0xA */ byte pad_A[2];
} GpState0C;
STATIC_ASSERT_SIZEOF(GpState0C, 0xC);

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
void func_800E3D8C(s32 arg0, s32 arg1);
void func_800E3EF0(s32 arg0);
void func_800E4080(void);
void func_800E40BC(s32 arg0, s32 arg1);
s16  func_800E6AD4(u16* arg0);
s32  func_800E6CE0(void);

void func_800E9BDC(u8 arg0, s32 arg1);
void func_800E9C6C(void);
s16  func_800EA1A8(VECTOR3* arg0, VECTOR3* arg1);
s32  func_800EA318(s16 arg0, s16 arg1, s16 arg2);
void func_800EA3A0(s32 arg0);
void func_800EA3B4(void);
void func_800EA3EC(void);
void func_800EA478(s32 arg0, GsCOORDINATE2* arg1, s32 arg2, s32 arg3);
void func_800EC7E4(void* arg0, Task* arg1);
void func_800EC868(void);
void func_800EC888(P_TAG* arg0, s32 arg1, s32 arg2);
void func_800EC9C8(void);
void func_800ECA10(s32 arg0);
void func_800ECA54(void);

#endif // GAMEPLAY_3CD8_H
