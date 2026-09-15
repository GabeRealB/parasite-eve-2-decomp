#include "common.h"

#include "actors/actors_shared_80137ea8.h"

void ActorsShared80137ea8(ActorShared80137ea8* arg0)
{
    u16                      scale;
    GsCOORDINATE2*           coord;
    ActorShared80137ea8Work* work;

    work                 = arg0->field_1C;
    coord                = &work->coord;
    scale                = work->field_34E - 2;
    work->field_34E      = scale;
    coord->coord.m[0][1] = (s16)((s32)(coord->coord.m[0][1] * (s16)scale) >> 0xC);
    coord->coord.m[1][1] = (s16)((s32)(coord->coord.m[1][1] * (s16)work->field_34E) >> 0xC);
    coord->coord.m[2][1] = (s16)((s32)(coord->coord.m[2][1] * (s16)work->field_34E) >> 0xC);
    work->coord.flg      = 0;
}
