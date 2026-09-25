#include "common.h"

#include "actors/actor_120400.h"

#include "main/task.h"
#include "main/tmd.h"

/// Message 0x7D5 handler of the parent: shows or hides its model. `mode`
/// drives the `TmdObject` parked in `Task::extra` -- bit 0x80 hides it, bit
/// 0x4 is the one the children copy alongside it:
///
///   mode 0  hide, drop 0x4
///   mode 1  show, `Tmd_AllocBuffers`, drop 0x4
///   mode 2  hide, start the `field_500` countdown to freeing the buffers, raise 0x4
///   mode 3  show, raise 0x4
///
/// Returns 0 for the four known modes and 1 for any other.
s32 func_actor_120400_80132C38(Task* task, s32 arg1, s32 mode, s32 arg3)
{
    TmdObject*           obj;
    Actor120400MainWork* work;
    s32                  ret;

    obj  = task->extra;
    work = (Actor120400MainWork*)task->work;
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
            work->field_500 = mode;
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
    return ret;
}

/// Message 0x7DB handler of the parent: ignores the message and returns 0.
s32 func_actor_120400_80132D14(void)
{
    return 0;
}
