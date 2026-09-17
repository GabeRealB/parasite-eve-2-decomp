#include "common.h"

#include "actors/actor_213000.h"

#include "main/task.h"
#include "main/tmd.h"

/// `Gp_DispatchMsg` handler: the four-way visibility/mode switch of
/// `func_actor_141000_80133E8C` run against the `TmdObject` parked in
/// `Task::extra`. Mode 0 shows the model and clears the 4 flag, 1 hides it,
/// frees the aux buffers and clears the flag, 2 does both plus latching the
/// mode into the work block's `field_477`, and 3 hides it while setting the
/// flag. Anything else returns 1 and leaves the object alone; the handled
/// modes return 0.
/// The handler reads `idMap` before the switch even though mode 2 is its only
/// use, so retail's `lw $v1,0x1C($a0)` sits in the entry block. The same body
/// shape as `func_actor_511000_801327A0` / `func_actor_350700_80162A14`.
s32 func_actor_213000_8014A8A4(Task* task, s32 arg1, s32 mode)
{
    TmdObject*       obj;
    Actor213000Work* work;
    s32              ret;

    obj  = task->extra;
    work = (Actor213000Work*)task->idMap;
    ret  = 0;

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
            work->field_477 = mode;
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
    return ret;
}

INCLUDE_ASM("actors/nonmatchings/actor_213000/actor_213000_3", func_actor_213000_8014A980);
