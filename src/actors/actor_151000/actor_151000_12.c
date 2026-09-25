#include "common.h"

#include "actors/actor_151000.h"
#include "main/task.h"
#include "main/tmd.h"

/// Visibility opcode: applies `arg2` to the model of the task published in
/// `D_actor_151000_8013D380` - bit 0 shows it (flags 0) rather than hiding it
/// (0x80), and bit 1 ORs in 0x4.
s32 func_actor_151000_801327C8(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* obj;

    obj = (TmdObject*)D_actor_151000_8013D380->extra;
    if (arg2 & 1) {
        obj->flags = 0;
    } else {
        obj->flags = 0x80;
    }
    if (arg2 & 2) {
        obj->flags |= 4;
    }
    return 0;
}
