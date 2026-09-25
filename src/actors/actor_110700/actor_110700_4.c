#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

/// Message 0x7D5 handler: sets the model's visibility from `arg2`. Bit 0 clear
/// replaces the object's flags with 0x80 (hidden), bit 0 set clears them
/// (shown); bit 1 then ORs in 0x4. `arg1` is not read.
s32 func_actor_110700_801320D8(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* obj;

    obj = (TmdObject*)task->extra;
    if (!(arg2 & 1)) {
        obj->flags = 0x80;
    } else {
        obj->flags = 0;
    }
    if (arg2 & 2) {
        obj         = (TmdObject*)task->extra;
        obj->flags |= 4;
    }
    return 0;
}
