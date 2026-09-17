#include "common.h"

#include "actors/actor_443500.h"

#include "main/task.h"
#include "main/tmd.h"

/// The four-way 0x7D5 switch documented on the prototype in `actor_443500.h`;
/// `ActorsShared80162bc4` carries the same body against its carriers' work
/// blocks. Only the mode-2 latch and the trailing mirror differ: the mode goes
/// into `Actor443500Work::field_4BC` -- the word the spawn handler seeds to -1
/// -- and the resulting `field_C` onto `field_4C0`.
s32 func_actor_443500_8013297C(Task* task, s32 anim, s32 mode, s32 arg3)
{
    TmdObject*       obj;
    s32              ret;
    Actor443500Work* work;

    obj  = task->extra;
    work = (Actor443500Work*)task->idMap;
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
            work->field_4BC = mode;
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
    work->field_4C0 = obj->field_C;
    return ret;
}

INCLUDE_ASM("actors/nonmatchings/actor_443500/actor_443500_6", func_actor_443500_80132A68);
