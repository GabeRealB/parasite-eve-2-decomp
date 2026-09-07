#include "common.h"

#include "actors/actors_shared_80134934.h"

void Gp_UpdateActorColor(void* arg0, VECTOR* arg1, s32 arg2, s32 arg3);

/// Same shape as `ActorsShared80134ff0`, but colours the actor from its
/// *second* attach coordinate: copies that coordinate's world position into a
/// stack `VECTOR` and hands it to `Gp_UpdateActorColor` with no blend
/// parameters.
void ActorsShared80134934(ActorShared80134934* arg0)
{
    GsCOORDINATE2* coord;
    VECTOR         vec;

    coord  = &arg0->field_2C->field_8[1];
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0->field_20, &vec, 0, 0);
}
