#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

/// Message 0x7D5 handler: switches the model's 0x80 flag: set when `arg2` is 0,
/// cleared for any other value. The message id itself is unused.
s32 func_actor_510900_8013BE64(Task* task, s32 msgId, s32 arg2)
{
    TmdObject* tmd;

    tmd = (TmdObject*)task->extra;
    if (arg2 == 0) {
        tmd->flags = 0x80;
    } else {
        tmd->flags = 0;
    }
    return 0;
}
