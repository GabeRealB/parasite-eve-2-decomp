#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

#include "actors/actors_shared_80164dd4.h"

void ActorsShared80164dd4(Task* arg0)
{
    ActorsShared80168d3cWork* work;
    s16                       angle;
    GsCOORDINATE2*            coord;
    ActorsShared80168d3cWork* anim;
    s32                       speed;
    s32                       dx;

    work                                            = (ActorsShared80168d3cWork*)arg0->idMap;
    angle                                           = work->field_40C;
    coord                                           = ((TmdObject*)arg0->extra)->field_8;
    dx                                              = rsin(angle) << 4;
    speed                                           = 0xC8;
    ((TmdObject*)arg0->extra)->field_8->coord.t[0] += (dx * speed) >> 16;
    ((TmdObject*)arg0->extra)->field_8->coord.t[2] += ((rcos(angle) << 4) * speed) >> 16;
    ((TmdObject*)arg0->extra)->field_8->flg         = 0;
    coord->coord.t[1]                              += work->field_42A;
    work->obj_2CC.field_12                         += work->field_42A;
    work->field_428                                += 0xE;
    work->field_42A                                += work->field_428;
    if (coord->coord.t[1] >= (s16)work->field_92) {
        anim                   = (ActorsShared80168d3cWork*)arg0->idMap;
        anim->field_426        = 2;
        anim->field_41C        = 0x10;
        anim->field_418        = 0x13;
        anim->field_414        = 1;
        coord->coord.t[1]      = (s16)work->field_92;
        work->obj_2CC.field_12 = 0;
        work->field_412        = 0;
        work->field_422++;
    }
}
