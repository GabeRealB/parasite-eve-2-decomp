#include "common.h"

#include "actors/actor_103700.h"

void func_actor_103700_801350DC(Task* task, s32 arg1, s32 arg2)
{
    Actor103700Work* work;
    GsCOORDINATE2*   coord;
    u16              frame;

    work  = (Actor103700Work*)task->work;
    coord = ((TmdObject*)task->extra)->coords;

    frame           = work->field_25E + 1;
    work->field_25E = frame;
    if (arg2 < (s16)frame) {
        work->field_25E = 0;
    }
    coord->coord.t[1] += D_actor_103700_80139DB8[(arg1 * 15) + (s16)work->field_25E];
}

void func_actor_103700_80135140(Task* task, s32 arg1)
{
    Actor103700Work* work;
    GsCOORDINATE2*   coord;
    u16              frame;
    s32              amp;

    work  = (Actor103700Work*)task->work;
    coord = ((TmdObject*)task->extra)->coords;

    frame           = work->field_25C + 1;
    work->field_25C = frame;
    if ((s16)frame >= 15) {
        work->field_25C = 0;
        Gp_LcgState     = Gp_LcgState * 5 + 0x71357911;
        work->field_25A = arg1 + ((Gp_LcgState >> 16) & 0x3F);
    }
    amp                = (work->field_25A * D_actor_103700_80139DF4[(s16)work->field_25C] * 16) >> 16;
    coord->coord.t[0] += (amp * coord->coord.m[0][0]) >> 12;
    coord->coord.t[2] += (amp * coord->coord.m[2][0]) >> 12;
}
