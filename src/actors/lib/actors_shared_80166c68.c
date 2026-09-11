#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

#include "actors/actors_shared_80166c68.h"
#include "actors/actors_shared_8016945c.h"

void ActorsShared80166c68(Task* arg0)
{
    ActorsShared80168d3cWork* work;
    ActorsShared80168d3cWork* work2;
    s32                       cond;
    s16                       angle;
    s16                       speed;
    s32                       scale;

    work = (ActorsShared80168d3cWork*)arg0->idMap;
    if ((u16)(work->field_412++ - 0x1D) < 0xD) {
        scale                                           = 0x1E;
        angle                                           = work->field_7A + 0x400;
        speed                                           = (((ActorsShared80168d3cWork*)arg0->idMap)->field_41C * scale) << 0xC >> 0x10;
        ((TmdObject*)arg0->extra)->field_8->coord.t[0] += ((rsin(angle) << 4) * speed) >> 0x10;
        ((TmdObject*)arg0->extra)->field_8->coord.t[2] += ((rcos(angle) << 4) * speed) >> 0x10;
        ((TmdObject*)arg0->extra)->field_8->flg         = 0;
    }
    work2 = (ActorsShared80168d3cWork*)arg0->idMap;
    if ((work2->flags_EC.half & 1) || (work2->flags_EC.word & 0x102)) {
        cond = 1;
    } else {
        cond = 0;
    }
    if (cond) {
        work->field_438 = 0;
        ActorsShared_SetTaskState(arg0, 3);
        ActorsShared_SetWorkState(arg0, 3);
    }
}
