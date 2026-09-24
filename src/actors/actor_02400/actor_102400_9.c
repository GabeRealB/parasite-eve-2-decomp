#include "common.h"

#include "actors/actors_shared_80134ff0.h"

void Gp_UpdateActorColor(void* arg0, VECTOR* arg1, s32 arg2, s32 arg3);

/// Refreshes the actor's colour from where its model stands: the root
/// coordinate's world position goes to `Gp_UpdateActorColor` with no blend.
void Actor02400_Fn031D0(ActorShared80134ff0* arg0)
{
    GsCOORDINATE2* coord;
    VECTOR         vec;

    coord  = arg0->field_2C->field_8;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0->field_20, &vec, 0, 0);
}
