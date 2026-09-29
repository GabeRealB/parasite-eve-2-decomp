#ifndef ACTOR_521100_PRIVATE_H
#define ACTOR_521100_PRIVATE_H

#include "gameplay/message.h"

#include "main/task_types.h"

typedef struct Actor521100FireRow {
    /* 0x0 */ s16 field_0;
    /* 0x2 */ u16 field_2;
} Actor521100FireRow;
STATIC_ASSERT_SIZEOF(Actor521100FireRow, 4);

extern Actor521100FireRow D_actor_521100_8015F80C[2][17];

typedef struct {
    s32 id;
    union {
        s16 (*call0)(Task *);
        s32 (*call1)(Task *);
        s32 (*call2)(Task *, s32, GpAnimArg *);
        s32 (*call3)(Task *, s32, GpCmdArg *);
        s32 (*call4)(Task *, s32, GpXformArg *);
        s32 (*call5)(Task *, s32, s32);
    } handler;
} Actor521100MessageEntry;
STATIC_ASSERT_SIZEOF(Actor521100MessageEntry, 8);

extern Actor521100MessageEntry D_actor_521100_8015F6FC[8];

s32 func_actor_521100_80135BEC(Task *);
s32 func_actor_521100_80135C14(Task *, s32, GpAnimArg *);
s32 func_actor_521100_80135CAC(Task *, s32, GpXformArg *);
s32 func_actor_521100_80135D10(Task *, s32, s32);
s32 func_actor_521100_80135D58(Task *, s32, GpCmdArg *);
s32 func_actor_521100_80135D9C(Task *);
s16 func_actor_521100_80135DC8(Task *);

#endif // ACTOR_521100_PRIVATE_H
