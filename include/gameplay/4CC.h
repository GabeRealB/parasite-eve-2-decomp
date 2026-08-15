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

s32 func_800BF5CC(Task* arg0, s32 arg1, GpItemObj2* arg2);

#endif // GAMEPLAY_4CC_H
