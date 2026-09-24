#include "common.h"

#include "actors/actor_521100.h"
#include "gameplay/3A34.h"

/// Colours the actor from the world position of its second model coordinate,
/// handing it to `Gp_UpdateActorColor` with no blend parameters.
void func_actor_521100_80135A34(Actor521100* arg0)
{
    GsCOORDINATE2* coord;
    VECTOR         vec;

    coord  = &arg0->field_2C->field_8[1];
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0->field_20, &vec, 0, 0);
}
