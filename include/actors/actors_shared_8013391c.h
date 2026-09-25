#ifndef ACTORS_SHARED_8013391C_H
#define ACTORS_SHARED_8013391C_H

#include "common.h"

#include "gameplay/message.h"
#include "main/task.h"

/// Work block the enemies carrying this body reach as `task->work`, which is
/// not a `TaskIdMap` here. The carriers allocate blocks of different sizes, so
/// only the halfword this body sets is described: the message selector's bit
/// is ORed into it, one bit each for selectors 1, 2 and 3.
typedef struct ActorsShared8013391cWork {
    /* 0x000 */ byte pad_0[0x33A];
    /* 0x33A */ s16  field_33A;
} ActorsShared8013391cWork;

/// Message 0x7DB handler shared by `actor_105300` and `actor_105400`, which
/// install it in their `GpMsgEntry` tables: ORs the status bit the payload's
/// selector names into the work block's `field_33A`. Selector 0 is a no-op.
s32 ActorsShared8013391c(Task* task, s32 msgId, GpCmdArg* msg);

#endif // ACTORS_SHARED_8013391C_H
