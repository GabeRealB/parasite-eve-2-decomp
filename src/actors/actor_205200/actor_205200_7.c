#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "actors/actor_205200.h"
#include "gameplay/3A34.h"

/// Feeds the actor's world position - the translation of its attach
/// coordinate - to `Gp_UpdateActorColor` for its enemy record, with no blend
/// parameters.
void func_actor_205200_8014C87C(Actor205200* arg0)
{
    GsCOORDINATE2* coord;
    VECTOR         vec;

    coord  = arg0->field_2C->field_8;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor((GpEnemy*)arg0->field_20, &vec, 0, 0);
}
