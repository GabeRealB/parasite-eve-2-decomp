#include "common.h"

#include "actors/actors_shared_8013806c.h"

void Gp_DrawEffGroundQuad(VECTOR3* arg0, s32 arg1, s16 arg2);

void ActorsShared8013806c(ActorShared8013806c* arg0)
{
    ActorShared8013806cWork* work;
    GsCOORDINATE2*           coord;
    GsCOORDINATE2*           sub;
    VECTOR3                  vec;

    work  = arg0->field_1C;
    coord = &arg0->field_2C->field_8->field_0;
    sub   = &arg0->field_2C->field_8->field_F0;
    if (work->field_6E2 == 0) {
        work->field_6E2 = -1;
    }
    vec.vx = sub->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = sub->workm.t[2];
    Gp_DrawEffGroundQuad(&vec, 0x300, work->field_6E2);
}
