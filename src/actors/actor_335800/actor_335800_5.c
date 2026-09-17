#include "common.h"

#include "actors/actor_335800.h"

#include "gameplay/gameplay.h"

#include "main/task.h"

#include "main/tmd.h"

/// State handler at index 1 of `D_actor_335800_80161E68`, the step after the
/// shared turn-to-face body `ActorsShared80133a68` -- the same slot
/// `func_actor_141000_80133B28` takes in its own table. The move body
/// `ActorsShared80132920` is: rotates the constant local-space offset
/// `D_actor_335800_80161E78` through the root part's matrix into
/// `work->step`, opens the per-axis stop threshold to 0x7FFF, which disables
/// it for the update loop, and advances `field_4C2` so the dispatcher
/// `func_actor_335800_80163B78` runs the next handler.
void func_actor_335800_80163CA0(Task* task)
{
    Actor335800Work* work;
    GsCOORDINATE2*   coord;
    VECTOR           vec;

    coord = ((TmdObject*)task->extra)->field_8;
    work  = (Actor335800Work*)task->idMap;

    vec = D_actor_335800_80161E78;
    ApplyMatrixLV(&coord->coord, &vec, (VECTOR*)&work->step);
    work->limit.vx = 0x7FFF;
    work->limit.vy = 0x7FFF;
    work->limit.vz = 0x7FFF;
    work->field_4C2++;
}

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_5", func_actor_335800_80163D20);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_5", func_actor_335800_80163E20);

INCLUDE_ASM("actors/nonmatchings/actor_335800/actor_335800_5", func_actor_335800_80163F3C);

s32 func_actor_335800_80163FB8(Task* task, s32 arg1, s32 mode)
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
            ((Actor335800Work*)task->idMap)->field_4C4 = mode;
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

s32 func_actor_335800_80164098(void)
{
    return 0;
}
