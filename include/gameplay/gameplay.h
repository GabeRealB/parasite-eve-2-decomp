#ifndef GAMEPLAY_GAMEPLAY_H
#define GAMEPLAY_GAMEPLAY_H

#include "common.h"

#include "main/session.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

/// Global at `D_80114C08`. `field_0` is a u16 loaded by many helpers.
/// `field_3` is a signed state byte (`lb`); `func_80109290` compares it to -2
/// and `func_80109374` requires 0. `field_6` is a flags byte (bit 0 gates
/// `func_800A7DB8` writing `field_E`). `field_A` is a signed byte (`lb`);
/// `func_800A7DE0` sets `field_3 = 2` when it is >= 2, then clears it.
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
    /* 0x0B */ byte pad_B[3];
    /* 0x0E */ u8   field_E;
    /* 0x0F */ byte pad_F;
} GpStateC08;
STATIC_ASSERT_SIZEOF(GpStateC08, 0x10);

extern GpStateC08 D_80114C08;

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

void func_80098F58(GsCOORDINATE2* arg0);
void func_80098F98(GsCOORDINATE2* arg0, s32 arg1);
Task* func_8009988C(GsCOORDINATE2* arg0);
u8*  func_800A746C(void);
void func_800A784C(void* arg0);
void func_800A7A64(void);
void func_800A7DB8(s32 arg0);
void func_800A7DE0(void);
void func_800A8B14(void);
void func_800A8B6C(void);
GpCb2CRec* func_800A8C08(GameSessionFrom4* arg0);
void func_800A8D5C(void);
void func_800A9010(Task* task);
void func_800A91CC(Task* task);

#endif // GAMEPLAY_GAMEPLAY_H
