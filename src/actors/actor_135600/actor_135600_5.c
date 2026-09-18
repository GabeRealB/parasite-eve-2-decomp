#include "common.h"

#include "actors/actor_135600.h"

#include "main/task.h"
#include "main/tmd.h"

/// The 0x7D5 entry of `D_actor_135600_8013B0F4`: the actor's own visibility,
/// switched on the word `mode` rather than on a pointer, and the same body
/// shape as `func_actor_350700_80163840`. Mode 0 shows the model and clears
/// the 4 flag, 1 hides it, frees the aux buffers and clears the flag, 2 does
/// both plus latching the mode into the work block's `field_508`, and 3 hides
/// it while setting the flag. Anything else returns 1 and leaves the object
/// alone; the handled modes return 0. Either way the resulting `field_C` is
/// republished onto the objects of the three child tasks the spawn handler
/// parked at `field_4FC` / `field_500` / `field_504`.
s32 func_actor_135600_80133240(Task* task, s32 msgId, s32 mode, s32 arg3)
{
    Actor135600Work* work;
    TmdObject*       obj;
    TmdObject*       objA;
    TmdObject*       objB;
    TmdObject*       objC;
    s32              ret;

    work = (Actor135600Work*)task->work;
    obj  = task->extra;
    objB = work->field_500->extra;
    objA = work->field_4FC->extra;
    objC = work->field_504->extra;
    ret  = 0;
    switch (mode) {
        case 0:
            obj->flags |= 0x80;
            obj->flags &= ~4;
            break;
        case 1:
            obj->flags &= ~0x80;
            Tmd_AllocBuffers(obj);
            obj->flags &= ~4;
            break;
        case 2:
            obj->flags     |= 0x80;
            work->field_508 = mode;
            obj->flags     |= 4;
            break;
        case 3:
            obj->flags &= ~0x80;
            obj->flags |= 4;
            break;
        default:
            ret = 1;
            break;
    }
    objB->flags = obj->flags;
    objA->flags = obj->flags;
    objC->flags = obj->flags;
    return ret;
}

s32 func_actor_135600_8013336C(void)
{
    return 0;
}
