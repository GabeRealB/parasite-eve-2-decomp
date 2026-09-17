#include "common.h"

#include "actors/actor_103700.h"

void func_actor_103700_801350DC(Task* task, s32 arg1, s32 arg2)
{
    Actor103700Work* work;
    GsCOORDINATE2*   coord;
    u16              frame;

    work  = (Actor103700Work*)task->idMap;
    coord = ((TmdObject*)task->extra)->field_8;

    frame           = work->field_25E + 1;
    work->field_25E = frame;
    if (arg2 < (s16)frame) {
        work->field_25E = 0;
    }
    coord->coord.t[1] += D_actor_103700_80139DB8[(arg1 * 15) + (s16)work->field_25E];
}

INCLUDE_ASM("actors/nonmatchings/actor_103700/actor_103700_5", func_actor_103700_80135140);
