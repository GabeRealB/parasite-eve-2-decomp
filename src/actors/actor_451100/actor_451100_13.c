#include "common.h"

#include "actors/actor_451100.h"
#include "main/task.h"
#include "main/tmd.h"

/// Message 0x7D5 handler of `D_actor_451100_8013F704`: bit 0 of `arg2` clears
/// `TmdObject::flags` on the model of the task in `D_actor_451100_8014E748`,
/// showing it, and its absence sets 0x80, hiding it; bit 1 additionally ORs in
/// 0x4.
s32 func_actor_451100_801325C8(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* obj;

    obj = (TmdObject*)D_actor_451100_8014E748->extra;
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
