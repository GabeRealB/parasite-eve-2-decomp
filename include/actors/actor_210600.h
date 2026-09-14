#ifndef ACTOR_210600_H
#define ACTOR_210600_H

#include "common.h"

#include "main/task.h"

/// Per-actor state block for the `actor_210600` overlay. `func_actor_210600_8014B8C8`
/// is the overlay's allocator: it calls `Mem_Calloc(0x8D8, 0)` and stores the
/// result in `Task::idMap` (0x1C), which an enemy actor reuses for its own work
/// block, so it is *not* a `TaskIdMap` here. The same function hands
/// `work + 0x898` and `work + 0x8B8` to the task's own 0x1C / 0x20 slots. The
/// size below is the allocation, not a guess; only the fields this overlay's
/// matched bodies touch are named.
typedef struct Actor210600Work {
    /* 0x000 */ byte pad_0[0x87C];
    /* 0x87C */ s16  field_87C;
    /* 0x87E */ byte pad_87E[0x4];
    /* 0x882 */ u16  field_882;
    /* 0x884 */ byte pad_884[0x2];
    /* 0x886 */ s16  field_886;
    /* 0x888 */ byte pad_888[0x8];
    /* 0x890 */ s16  field_890;
    /* 0x892 */ byte pad_892[0x46];
} Actor210600Work;
STATIC_ASSERT_SIZEOF(Actor210600Work, 0x8D8);

/// Payload the sender of message 0x7DB passes as `Gp_DispatchMsg`'s `arg2`;
/// the same 4-byte record as `Actor143900Msg` and `Actor560800Msg`.
typedef struct Actor210600Msg {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} Actor210600Msg;
STATIC_ASSERT_SIZEOF(Actor210600Msg, 0x4);

s32 func_actor_210600_8014B770(Task* task, s32 msgId, Actor210600Msg* msg);

#endif // ACTOR_210600_H
