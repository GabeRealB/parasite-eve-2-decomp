#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

#include "actors/actors_shared_80168010.h"

void ActorsShared80168010(Task* arg0)
{
    ActorsShared80168d3cWork* work;
    s16                       angle;
    GsCOORDINATE2*            coord;
    ActorsShared80168d3cWork* anim;
    s32                       speed;
    s32                       dx;

    work                                            = (ActorsShared80168d3cWork*)arg0->idMap;
    angle                                           = work->field_7A;
    coord                                           = ((TmdObject*)arg0->extra)->field_8;
    dx                                              = rsin(angle) << 4;
    speed                                           = -0x8C;
    ((TmdObject*)arg0->extra)->field_8->coord.t[0] += (dx * speed) >> 16;
    ((TmdObject*)arg0->extra)->field_8->coord.t[2] += ((rcos(angle) << 4) * speed) >> 16;
    ((TmdObject*)arg0->extra)->field_8->flg         = 0;
    work->field_78                                 += (0x200 - work->field_78) >> 5;
    coord->coord.t[1]                              += work->field_42A;
    work->field_428                                += 2;
    work->field_42A                                += work->field_428;
    if (coord->coord.t[1] > 0) {
        work->field_451   = 0;
        work->field_412   = 0;
        coord->coord.t[1] = -0x3C;
        anim              = (ActorsShared80168d3cWork*)arg0->idMap;
        anim->field_41C   = 0x10;
        anim->field_418   = 0xC;
        anim->field_414   = 2;
        work->field_428   = 0;
        work->field_42A   = -0x12C;
        work->field_420++;
    }
}
