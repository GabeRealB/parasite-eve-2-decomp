#include "common.h"

#include "actors/actor_350500.h"

#include "main/task.h"
#include "main/tmd.h"

/// `Gp_DispatchMsg` handler: the four-way visibility/mode switch on the
/// message's mode word, run against the `TmdObject` parked in `Task::extra`.
/// Mode 0 hides the model (flag 0x80, under which the tick also skips the
/// shadow and the part update) and clears the 4 flag; 1 shows it, allocates
/// the model buffers and clears 4; 2 hides it, sets 4 and latches the mode
/// into the `field_4C5` countdown, which frees the buffers when it runs out;
/// 3 shows it and sets 4. Anything else returns 1 and leaves the object
/// alone; the handled modes return 0.
s32 func_actor_350500_801629DC(Task* task, s32 arg1, s32 mode)
{
    TmdObject* obj;
    s32        ret;

    obj = task->extra;
    ret = 0;
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
            obj->flags                               |= 0x80;
            ((Actor350500Work*)task->work)->field_4C5 = mode;
            obj->flags                               |= 4;
            break;
        case 3:
            obj->flags &= ~0x80;
            obj->flags |= 4;
            break;
        default:
            ret = 1;
            break;
    }
    return ret;
}

/// `Gp_DispatchMsg` handler: latches the variant the message's halfword at
/// 0x2 selects into `field_4C4` -- 1 clears it, 2 sets it, anything else
/// leaves it. Always returns 0.
s32 func_actor_350500_80162ABC(Task* task, s32 arg1, Actor350500Msg* msg)
{
    Actor350500Work* work;

    work = (Actor350500Work*)task->work;
    switch (msg->field_2) {
        case 1:
            work->field_4C4 = 0;
            break;
        case 2:
            work->field_4C4 = 1;
            break;
    }
    return 0;
}
