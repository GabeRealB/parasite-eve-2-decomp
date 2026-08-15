#ifndef GAMEPLAY_4CC_H
#define GAMEPLAY_4CC_H

#include "common.h"

#include "main/task.h"

/// Object at `Task::spawnArg2` for `func_800BF5CC`. `field_8` is the packed
/// item id passed to `func_800BB470`.
typedef struct _GpItemObj8 {
    /* 0x00 */ byte pad_0[8];
    /* 0x08 */ u8   field_8;
} GpItemObj8;

/// Object stored in `Task::extraState` by `func_800BF5CC`. `field_2` is set
/// when `func_800BB470` returns 2.
typedef struct _GpItemObj2 {
    /* 0x00 */ byte pad_0[2];
    /* 0x02 */ u8   field_2;
} GpItemObj2;

/// 8-byte item descriptor in `D_8010D838`, indexed by item id.
/// `field_3` bit 0 gates the `arg1 == 1` result in `func_800BF334`.
/// `field_4` is a name/string pointer walked by `func_800B8EB0`.
typedef struct _GpItemDesc {
    /* 0x00 */ u8    field_0;
    /* 0x01 */ u8    field_1;
    /* 0x02 */ u8    field_2;
    /* 0x03 */ u8    field_3;
    /* 0x04 */ void* field_4;
} GpItemDesc;
STATIC_ASSERT_SIZEOF(GpItemDesc, 0x8);

extern GpItemDesc D_8010D838[];
/// Item/location halfword copied from `D_80114DDC` by `func_800BCC44`.
/// `func_800BF334` special-cases the value `0x703`.
extern u16 D_80114D7C;
/// Byte 3 of the `D_8007216C` location key (`GameSession.field_7`).
extern u8 D_8007216F;

s32 func_800BF334(s32 arg0, s32 arg1);
s32 func_800BF5CC(Task* arg0, s32 arg1, GpItemObj2* arg2);

#endif // GAMEPLAY_4CC_H
