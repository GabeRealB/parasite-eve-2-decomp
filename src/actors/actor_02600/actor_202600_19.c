#include "common.h"

#include "actors/actor_202600.h"
#include "gameplay/3A34.h"

/// Passes the world position of the model's root coordinate to
/// `Gp_UpdateActorColor` for the context, with both trailing arguments 0.
void Actor02600_Fn03A68(Actor202600* arg0)
{
    GsCOORDINATE2* coord;
    VECTOR         vec;

    coord  = arg0->field_2C->field_8;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor((GpEnemy*)arg0->field_20, &vec, 0, 0);
}
