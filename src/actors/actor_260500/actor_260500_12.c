#include "common.h"

#include "actors/actor_260500.h"
#include "main/tmd.h"

/// Visibility handler: bit 0 of `arg2` shows the actor's model (flags 0) or
/// hides it (0x80), and bit 1 ORs in 0x4.
s32 func_actor_260500_8014A754(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* obj;

    obj = (TmdObject*)D_actor_260500_80159E50->extra;
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
