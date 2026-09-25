#include "common.h"

#include "actors/actor_312200.h"
#include "main/tmd.h"

/// Id 0x7D5 command handler, listed first in `D_actor_312200_80169F5C`. `arg2`
/// is the mode: 0 sets the model's `TmdObject::flags` to exactly 0x80, 1 clears
/// them, 2 raises bit 0x4, and 3 clears them and then raises bit 0x4. Modes 0
/// and 1 re-run `Tmd_AllocBuffers` on the model, and every mode except 1 resets
/// the work block's `field_0` state word. `arg1` is unused.
s32 func_actor_312200_80163510(Task* task, s32 arg1, s32 arg2)
{
    TmdObject*       obj;
    Actor312200Work* work;

    obj  = (TmdObject*)task->extra;
    work = (Actor312200Work*)task->work;
    switch (arg2) {
        case 0:
            obj->flags = 0x80;
            Tmd_AllocBuffers(obj);
            work->field_0 = 0;
            break;
        case 1:
            obj->flags = 0;
            Tmd_AllocBuffers(obj);
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
