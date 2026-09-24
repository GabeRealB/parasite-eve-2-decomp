#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

/// Shows the task's model when `arg2` is non-zero and hides it (bit 0x80 of
/// its `TmdObject` flags) otherwise; `arg1` is unused.
void func_actor_136100_801346EC(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* obj;

    obj = (TmdObject*)task->extra;
    if (arg2 != 0) {
        obj->flags = obj->flags & 0xFF7F;
        return;
    }
    obj->flags = obj->flags | 0x80;
}
