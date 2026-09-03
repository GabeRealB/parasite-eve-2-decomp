#ifndef ROOMS_ACROPOLIS_HELICOPTER_LANDING_PAD_H
#define ROOMS_ACROPOLIS_HELICOPTER_LANDING_PAD_H

#include "common.h"

#include "main/task.h"
#include "rooms/room_common.h"

#include <psyq/libgs.h>
#include <psyq/libgte.h>

/// 0x54 work block of the helipad enemy task, hung off the `Task::idMap`
/// slot -- it is the `Mem_Calloc(0x54)` block that
/// `func_acropolis_helicopter_landing_pad_8017D658` allocates, not a
/// `TaskIdMap`. Reach it with `(AhlpEnemyWork*)task->idMap`.
///
/// `lightMtx` / `colorMtx` are the model's own flat-light matrices:
/// `func_acropolis_helicopter_landing_pad_8017D7B0` points the `TmdObject`'s
/// `field_1C` / `field_20` at them and fills them from the three
/// `D_acropolis_helicopter_landing_pad_80182340` lights.
typedef struct AhlpEnemyWork {
    /* 0x00 */ s32    field_0;
    /* 0x04 */ s32    field_4;
    /* 0x08 */ s32    field_8;
    /* 0x0C */ s32    field_C;
    /* 0x10 */ MATRIX lightMtx;
    /* 0x30 */ MATRIX colorMtx;
    /* 0x50 */ s16    field_50;
    /* 0x52 */ byte   pad_52[0x2];
} AhlpEnemyWork;
STATIC_ASSERT_SIZEOF(AhlpEnemyWork, 0x54);

#endif
