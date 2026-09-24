#include "common.h"

#include "actors/actor_403000.h"
#include "main/task.h"
#include "main/tmd.h"

/// Handler for message 0x7D5 in the actor's message table. `arg2` picks the
/// display mode: 0 hides the model (flag 0x80) and 1 shows it again, both
/// re-running `Tmd_AllocBuffers`; 2 sets flag 0x4 on top of the current flags
/// and 3 replaces them with just 0x4. Every mode but 1 resets the animation
/// state `field_0`. `arg1` is unused.
s32 func_actor_403000_8013D268(Task* task, s32 arg1, s32 arg2)
{
    TmdObject*       obj;
    Actor403000Work* work;

    obj  = (TmdObject*)task->extra;
    work = (Actor403000Work*)task->work;
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
