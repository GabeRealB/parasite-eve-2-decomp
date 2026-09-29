#ifndef ACTOR_510900_PRIVATE_H
#define ACTOR_510900_PRIVATE_H

#include "gameplay/message.h"

#include "main/task_types.h"

typedef struct {
    s32 id;
    union {
        s16 (*call0)(Task *);
        s32 (*call1)(Task *);
        s32 (*call2)(Task *, s32, GpAnimArg *);
        s32 (*call3)(Task *, s32, GpXformArg *);
        s32 (*call4)(Task *, s32, s32);
    } handler;
} Actor510900MessageEntry;
STATIC_ASSERT_SIZEOF(Actor510900MessageEntry, 8);

extern Actor510900MessageEntry D_actor_510900_80167A6C[7];

s32 func_actor_510900_801391B8(Task *, s32, s32);
s32 func_actor_510900_8013BD5C(Task *);
s32 func_actor_510900_8013BD84(Task *, s32, GpAnimArg *);
s32 func_actor_510900_8013BE00(Task *, s32, GpXformArg *);
s32 func_actor_510900_8013BE64(Task *, s32, s32);
s16 func_actor_510900_8013BE84(Task *);

#endif // ACTOR_510900_PRIVATE_H
