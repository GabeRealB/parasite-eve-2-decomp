#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/1BC.h"

#include "actors/actor_105100.h"

/// Relights the actor at its model's world position: copies the model
/// coordinate's translation into a `VECTOR` and hands it with the context to
/// `Gp_UpdateActorColor`, with no blend parameters.
void func_actor_105100_801364CC(Actor105100* arg0)
{
    GsCOORDINATE2* coord;
    VECTOR         vec;

    coord  = arg0->field_2C->field_8;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor((GpEnemy*)arg0->field_20, &vec, 0, 0);
}
