#include "common.h"

#include "actors/actor_141000.h"

#include "main/task.h"
#include "main/tmd.h"

/// `Gp_DispatchMsg` handler: the four-way visibility/mode switch on the
/// message's mode word, run against the `TmdObject` parked in `Task::extra`.
/// Mode 0 shows the model and clears the 4 flag, 1 hides it, frees the aux
/// buffers and clears the flag, 2 does both plus latching the mode into the
/// work block's `field_4C9`, and 3 hides it while setting the flag. Anything
/// else returns 1 and leaves the object alone; the handled modes return 0.
/// The same body shape as `func_actor_503500_80132584`.
s32 func_actor_141000_80133E8C(Task* task, s32 arg1, s32 mode)
{
    TmdObject* obj;
    s32        ret;

    obj = task->extra;
    ret = 0;
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
            obj->field_C                              |= 0x80;
            ((Actor141000Work*)task->idMap)->field_4C9 = mode;
            obj->field_C                              |= 4;
            break;
        case 3:
            obj->field_C &= ~0x80;
            obj->field_C |= 4;
            break;
        default:
            ret = 1;
            break;
    }
    return ret;
}

s32 func_actor_141000_80133F6C(Task* task, s32 arg1, Actor141000Msg* msg)
{
    Actor141000Work* work;

    work = (Actor141000Work*)task->idMap;
    switch (msg->field_2) {
        case 1:
            work->field_4C8 = 0;
            break;
        case 2:
            work->field_4C8 = 1;
            break;
    }
    return 0;
}

INCLUDE_ASM("actors/nonmatchings/actor_141000/actor_141000_5", func_actor_141000_80133FA8);
