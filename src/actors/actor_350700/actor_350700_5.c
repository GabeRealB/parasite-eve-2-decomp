#include "common.h"
#include "actors/actor_350700.h"
#include "main/task.h"
#include "main/tmd.h"
#include "gameplay/1BC.h"

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700_5", func_actor_350700_801635A8);

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700_5", func_actor_350700_801636A8);

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700_5", func_actor_350700_801637C4);

/// `Gp_DispatchMsg` handler: the four-way visibility/mode switch on the
/// message's mode word, run against the `TmdObject` parked in `Task::extra`,
/// then the resulting `field_C` flags are republished onto the objects of the
/// three child tasks the spawn handler parked at `field_4FC` / `field_500` /
/// `field_504`. Mode 0 shows the model and clears the 4 flag, 1 hides it, frees
/// the aux buffers and clears the flag, 2 does both plus latching the mode into
/// the work block's `field_508`, and 3 hides it while setting the flag.
/// Anything else returns 1 and leaves the object alone; the handled modes
/// return 0. The same body shape as `func_actor_335800_8016343C`, over one more
/// child.
s32 func_actor_350700_80163840(Task* task, s32 arg1, s32 mode)
{
    Actor350700MainWork* work;
    TmdObject*           obj;
    TmdObject*           objA;
    TmdObject*           objB;
    TmdObject*           objC;
    u16                  flags;
    s32                  ret;

    work = (Actor350700MainWork*)task->work;
    obj  = task->extra;
    objA = work->field_4FC->extra;
    objB = work->field_500->extra;
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
    flags       = obj->flags;
    objB->flags = flags;
    objA->flags = flags;
    objC->flags = flags;
    return ret;
}

s32 func_actor_350700_8016395C(void)
{
    return 0;
}
