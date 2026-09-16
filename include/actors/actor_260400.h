#ifndef ACTOR_260400_H
#define ACTOR_260400_H

#include "common.h"

#include "main/task.h"

/// Work block this overlay hangs off the task's `Task::idMap` slot (0x1C),
/// which is not a `TaskIdMap` here. The overlay's state-0 handler
/// (`ActorsShared80131f9cSub0`, here at 0x80149FE0) allocates it
/// with `Mem_Calloc(0x4F8, 0)` and stores it straight into that field, so the
/// size below is the allocation and not a guess. Reach it with
/// `(Actor260400Work*)task->idMap`.
///
/// The task at +0x4F0 is the helper task this actor spawns; the exit callback
/// `func_actor_260400_8014A630` kills it on teardown.
typedef struct Actor260400Work {
    /* 0x000 */ byte  pad_0[0x4EC];
    /* 0x4EC */ s16   field_4EC; // animation reset argument, latched to 0x14 by the case-0 branch of `func_actor_260400_8014AAA4`
    /* 0x4EE */ byte  pad_4EE[0x2];
    /* 0x4F0 */ Task* field_4F0;
    /* 0x4F4 */ s8    field_4F4; // case selector `func_actor_260400_8014AAA4` mirrors out of the message
    /* 0x4F5 */ byte  pad_4F5[0x3];
} Actor260400Work;
STATIC_ASSERT_SIZEOF(Actor260400Work, 0x4F8);

/// The overlay's message payload, the `arg2` of `func_actor_260400_8014AAA4`.
/// Same 4-byte id/count record as `Actor143900Msg`: the halfword at 0x2 picks
/// the branch, the halfword at 0 holds the other id.
typedef struct Actor260400Msg {
    /* 0x0 */ u16 field_0;
    /* 0x2 */ u16 field_2;
} Actor260400Msg;
STATIC_ASSERT_SIZEOF(Actor260400Msg, 0x4);

/// The block above, published by `ActorsShared80131f9c` from the task's
/// `Task::idMap`. Declared here with the type the overlay reads it through,
/// the same way `include/actors/actor_143900.h` does; the shared header
/// publishes the bare `void*`.
extern Actor260400Work* ActorsShared80131f9cWork;

s32 func_actor_260400_8014AAA4(Task* task, s32 arg1, Actor260400Msg* msg);

#endif
