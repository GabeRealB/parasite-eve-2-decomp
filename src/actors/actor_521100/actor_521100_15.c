#include "common.h"

#include "actors/actor_521100.h"
#include "main/task.h"
#include "main/tmd.h"

/// Message 0x7D5 handler in `D_actor_521100_8016A358`: shows or hides the model of `D_actor_521100_8016A3DC`.
/// Bit 0 of `arg2` selects `TmdObject::flags` 0 (shown) or 0x80 (hidden), and
/// bit 1 ORs in 0x4.
s32 func_actor_521100_80136A1C(Task* task, s32 arg1, s32 arg2)
{
    TmdObject* obj;

    obj = (TmdObject*)D_actor_521100_8016A3DC->extra;
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
