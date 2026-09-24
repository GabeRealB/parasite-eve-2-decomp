#include "common.h"

#include "actors/actor_101500.h"
#include "gameplay/3A34.h"

/// Hands `Gp_UpdateActorColor` the world position of the model's second
/// coordinate, with no blend parameters.
void Actor01500_Fn02B14(Actor101500* arg0)
{
    GsCOORDINATE2* coord;
    VECTOR         vec;

    coord  = &arg0->field_2C->coords[1];
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0->field_20, &vec, 0, 0);
}
