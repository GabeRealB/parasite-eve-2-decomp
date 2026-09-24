#include "common.h"

#include "actors/actors_shared_80137ea8.h"

/// Flattens the coordinate `Actor07000_Fn05FF8` spliced in: the Y scale
/// `field_34E` loses 2 and the matrix's second column is scaled by it, then
/// the coordinate is marked dirty.
void Actor07000_Fn06088(ActorShared80137ea8* arg0)
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
