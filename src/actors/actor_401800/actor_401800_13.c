#include "common.h"

#include "actors/actor_401800.h"
#include "main/tmd.h"

/// Applies one of four model settings chosen by `arg2`: 0 sets the model's
/// flags to 0x80 and reallocates its buffers, 1 clears the flags and
/// reallocates them, 2 and 3 set bit 2 (3 clearing the rest first). Case 1
/// moves the actor to state 0x18, the others to state 0. Always returns 0.
s32 func_actor_401800_8013DD2C(Task* task, s32 arg1, s32 arg2)
{
    TmdObject*       obj;
    Actor401800Work* work;

    obj  = (TmdObject*)task->extra;
    work = (Actor401800Work*)task->work;
    switch (arg2) {
        case 0:
            obj->flags = 0x80;
            Tmd_AllocBuffers(obj);
            work->field_0 = 0;
            break;
        case 1:
            obj->flags = 0;
            Tmd_AllocBuffers(obj);
            work->field_0 = 0x18;
            break;
        case 2:
            obj->flags   |= 4;
            work->field_0 = 0;
            break;
        case 3:
            obj->flags    = 0;
            work->field_0 = 0;
            obj->flags   |= 4;
            break;
    }
    return 0;
}
