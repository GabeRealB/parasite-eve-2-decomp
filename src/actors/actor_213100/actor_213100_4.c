#include "common.h"
#include "actors/actor_213100.h"

#include "main/task.h"
#include "main/tmd.h"

/// Message-0x7D5 display handler: the four-way visibility/mode switch on the
/// message's mode word, run against the `TmdObject` parked in `Task::extra`
/// and mirrored onto the `Task::extra` of the child parked in the work block's
/// `field_480`. Mode 0 shows the model (`field_C` bit 0x80) and clears the 4
/// flag, 1 hides it, reinstates the aux buffers through `Tmd_AllocBuffers` and
/// clears the flag, 2 hides it and latches the mode into the work block's
/// `field_484`, and 3 hides it while setting the flag. Anything else returns 1
/// and leaves the object alone; the handled modes return 0.
/// The same body shape as `func_actor_135400_801327E8`, which calls
/// `Tmd_FreeBuffers` where this one latches `field_484`.
s32 func_actor_213100_8014A40C(Task* task, s32 arg1, s32 mode)
{
    TmdObject*       obj;
    TmdObject*       other;
    Actor213100Work* work;
    s32              ret;

    obj   = (TmdObject*)task->extra;
    work  = (Actor213100Work*)task->work;
    other = (TmdObject*)work->field_480->extra;
    ret   = 0;
    switch (mode) {
        case 0:
            obj->field_C |= 0x80;
            obj->field_C &= ~4;
            break;
        case 1:
            obj->field_C &= ~0x80;
            Tmd_AllocBuffers(obj);
            obj->field_C &= ~4;
            break;
        case 2:
            obj->field_C   |= 0x80;
            work->field_484 = mode;
            obj->field_C   |= 4;
            break;
        case 3:
            obj->field_C &= ~0x80;
            obj->field_C |= 4;
            break;
        default:
            ret = 1;
            break;
    }
    other->field_C = obj->field_C;
    return ret;
}
