#include "common.h"

#include "actors/actor_317000.h"

#include "main/task.h"
#include "main/tmd.h"

/// Message 0x7D5 handler of `D_actor_317000_8016CF50`, also called directly by
/// the spawn state `func_actor_317000_8016267C` with mode 0. `mode` sets or
/// clears bit 0x80 of `TmdObject::flags` and sets or clears bit 0x4:
///
///   mode 0  set 0x80, clear 0x4
///   mode 1  clear 0x80, `Tmd_AllocBuffers`, clear 0x4
///   mode 2  set 0x80, store 2 in the countdown `Actor317000Work::field_4C8`
///           that `func_actor_317000_80161E68` ends in `Tmd_FreeBuffers`,
///           set 0x4
///   mode 3  clear 0x80, set 0x4
///
/// Any other mode returns 1; the four known ones return 0. `arg3` is unused.
s32 func_actor_317000_80162BC4(Task* task, s32 arg1, s32 mode, s32 arg3)
{
    TmdObject*       obj;
    Actor317000Work* work;
    s32              ret;

    obj  = task->extra;
    work = (Actor317000Work*)task->work;
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
            work->field_4C8 = mode;
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
