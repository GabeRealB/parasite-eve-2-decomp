#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

#include "actors/actors_shared_80166f54.h"
#include "actors/actors_shared_8016945c.h"

void ActorsShared80166f54(Task* arg0)
{
    ActorsShared80168d3cWork* work;
    ActorsShared80168d3cWork* work2;
    ActorsShared80168d3cWork* next;
    s32                       cond;
    s16                       angle;
    s16                       speed;
    s32                       scale;

    work = (ActorsShared80168d3cWork*)arg0->idMap;
    if ((ActorsShared8016945c(arg0) << 0x10) == 0) {
        if ((u16)(work->field_412++ - 0x17) < 0xD) {
            scale                                           = -0x1E;
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
            next            = (ActorsShared80168d3cWork*)arg0->idMap;
            next->field_420 = 0;
            next->field_422 = 0;
        }
    }
}
