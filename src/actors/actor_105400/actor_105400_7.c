#include "common.h"

#include "actors/actor_105400.h"
#include "gameplay/3A34.h"

/// Hands the model's world position (its coordinate's `workm` translation) to
/// `Gp_UpdateActorColor` for the enemy, with no blend parameters.
void func_actor_105400_801335B8(Actor05400* arg0)
{
    GsCOORDINATE2* coord;
    VECTOR         vec;

    coord  = arg0->field_2C->field_8;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(arg0->field_20, &vec, 0, 0);
}
