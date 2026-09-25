#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

/// Visibility message handler: bit 0 of `arg2` shows the model (flags 0)
/// instead of hiding it (0x80); bit 1 also sets flag 0x4.
s32 func_actor_511000_80133EAC(Task* task, s32 arg1, s32 arg2)
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
