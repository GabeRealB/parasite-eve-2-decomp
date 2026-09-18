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
/// The handler reads `work` before the switch even though mode 2 is its only
/// use, so retail's `lw $v1,0x1C($a0)` sits in the entry block. The same body
/// shape as `func_actor_511000_801327A0` / `func_actor_350700_80162A14`.
s32 func_actor_213000_8014A8A4(Task* task, s32 arg1, s32 mode)
{
    TmdObject*       obj;
    Actor213000Work* work;
    s32              ret;

    obj  = task->extra;
    work = (Actor213000Work*)task->work;
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

/// Message handler: the visibility switch of the two model tasks the work
/// block parks at `field_4BC` / `field_4C0`. Mode 0 hides the first model
/// (clears bit 0x80 of the `TmdObject::field_C` parked in that task's
/// `Task::extra`) and 1 shows it; 2 and 3 hide and show the second. An
/// unknown mode touches nothing. Every path returns 0.
/// Both `field_C |= 0x80` arms are written out in the source; the post-reload
/// `jump2` cross-jump folds mode 1's copy into mode 3's, which is why retail's
/// mode-1 arm is only the `lw` plus a jump while modes 0 and 2 each keep their
/// own `& 0xFF7F` copy. Which tails jump2 merges is decided by which jumps
/// share a target label, not by how alike the bodies are.
s32 func_actor_213000_8014A980(Task* task, s32 arg1, Actor213000Msg* msg)
{
    Actor213000Work* work;
    Task*            child;
    u16              mode;

    mode = msg->field_2;
    work = (Actor213000Work*)task->work;

    switch (mode) {
        case 0:
            child = work->field_4BC;
            if (child != NULL) {
                ((TmdObject*)child->extra)->field_C &= 0xFF7F;
            }
            break;
        case 1:
            child = work->field_4BC;
            if (child != NULL) {
                ((TmdObject*)child->extra)->field_C |= 0x80;
            }
            break;
        case 2:
            child = work->field_4C0;
            if (child != NULL) {
                ((TmdObject*)child->extra)->field_C &= 0xFF7F;
            }
            break;
        case 3:
            child = work->field_4C0;
            if (child != NULL) {
                ((TmdObject*)child->extra)->field_C |= 0x80;
            }
            break;
    }
    return 0;
}
