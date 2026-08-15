#ifndef GAMEPLAY_GAMEPLAY_H
#define GAMEPLAY_GAMEPLAY_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

/// Global at `D_80114C08`. `field_0` is a u16 loaded by many helpers.
/// `field_3` is a signed state byte (`lb`); `func_80109290` compares it to -2
/// and `func_80109374` requires 0. `field_6` is a flags byte (bit 0 gates
/// `func_800A7DB8` writing `field_E`).
typedef struct _GpStateC08 {
    /* 0x00 */ u16  field_0;
    /* 0x02 */ byte pad_2;
    /* 0x03 */ s8   field_3;
    /* 0x04 */ byte pad_4[2];
    /* 0x06 */ u8   field_6;
    /* 0x07 */ byte pad_7[7];
    /* 0x0E */ u8   field_E;
    /* 0x0F */ byte pad_F;
} GpStateC08;
STATIC_ASSERT_SIZEOF(GpStateC08, 0x10);

extern GpStateC08 D_80114C08;

void func_80098F58(GsCOORDINATE2* arg0);
void func_80098F98(GsCOORDINATE2* arg0, s32 arg1);
void func_800A784C(void* arg0);
void func_800A7A64(void);
void func_800A7DB8(s32 arg0);
void func_800A8D5C(void);

#endif // GAMEPLAY_GAMEPLAY_H
