#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

/// Message 0x7D5 handler, listed in `D_actor_205200_801567D0` beside the 0x7DB
/// one: `arg2` zero sets the 0x80 flag of the task's model and any other value
/// clears its flags. The opcode itself (`msgId`) is unused.
s32 func_actor_205200_8014C980(Task* task, s32 msgId, s32 arg2)
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
