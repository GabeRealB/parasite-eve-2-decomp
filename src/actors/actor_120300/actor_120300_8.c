#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

/// Message 0x7D5 handler: a nonzero `arg2` shows the task's model (clears
/// `TmdObject` flag 0x80), zero hides it. `arg1` is the message id.
void func_actor_120300_80133C38(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* obj;

    obj = (TmdObject*)task->extra;
    if (arg2 != 0) {
        obj->flags = obj->flags & 0xFF7F;
        return;
    }
    obj->flags = obj->flags | 0x80;
}
