#include "common.h"

#include "actors/actor_461800.h"
#include "main/task.h"
#include "main/tmd.h"

/// Visibility message of the second variant: applies `arg2` to the model of
/// the task published in `D_actor_461800_801438A4` - bit 0 selects
/// `TmdObject.flags` 0 (shown) vs 0x80 (hidden), bit 1 ORs in 0x4.
s32 func_actor_461800_80133928(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* obj;

    obj = (TmdObject*)D_actor_461800_801438A4->extra;
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
