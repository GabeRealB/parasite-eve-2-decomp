#ifndef ACTOR_548100_H
#define ACTOR_548100_H

#include "common.h"

#include "main/task.h"

/// Per-instance work block of actor_548100, parked in `Task::idMap` -- that
/// slot is not a `TaskIdMap` here, it is the `Mem_Calloc(0x18, 0)` block
/// `func_actor_548100_80132420` allocates at spawn and stores at
/// `Task::idMap` (0x1C). Reach it with `(Actor548100Work*)task->idMap`.
///
/// `step` is the actor's 1-based progress step (0 while unset): the spawner
/// `func_actor_548100_80132550` seeds it from the `field_8` of the first
/// `D_actor_548100_801357E8` record whose `field_B` is set, and
/// `func_actor_548100_80132684` switches on it with nine cases. The game-flag
/// nibble recording whether a step is done is `step + 0xBE`, which is why the
/// same `+ 0xBE` shows up at every `GameFlag_GetNibble` / `GameFlag_SetNibble`
/// site in the overlay. `bit2Slot` is the 2-bit slot this instance occupies in
/// the current `Gp_Bit2Banks` word (`Gp_GetCurBit2Flag` / `Gp_SetCurBit2Flag`,
/// seeded with 5 or 4 by `func_actor_548100_80132684`) and `collectBitId` an id
/// in the `Gp_ClearCollectedBit` space (0x120 for the instance that reaches
/// `Gp_StartCapSlot` with kind 1).
///
/// 0x8 and up is the ramp `func_actor_548100_80134FEC` drives: `field_8` is the
/// period, `field_A` the elapsed counter it advances by 4 and clamps to
/// `field_8`, `field_C` the value that period ramps to, `field_E` the
/// interpolated result `field_C * field_A / field_8`, and `field_10` / `field_12`
/// a second period/elapsed pair on the same shape. The names stop at the last
/// field those bodies reach; the block is 0x18 bytes in full.
typedef struct Actor548100Work {
    /* 0x00 */ byte pad_0[0x2];
    /* 0x02 */ s16  step;
    /* 0x04 */ s16  collectBitId;
    /* 0x06 */ s8   field_6;
    /* 0x07 */ s8   bit2Slot;
    /* 0x08 */ s16  field_8;
    /* 0x0A */ s16  field_A;
    /* 0x0C */ s16  field_C;
    /* 0x0E */ s16  field_E;
    /* 0x10 */ s16  field_10;
    /* 0x12 */ s16  field_12;
    /* 0x14 */ byte pad_14[0x4];
} Actor548100Work;
STATIC_ASSERT_SIZEOF(Actor548100Work, 0x18);

#endif
