#include "common.h"

#include "actors/actor_103700.h"
#include "gameplay/3A34.h"

/// Refreshes the actor's colour from the world position of its root
/// coordinate, with no blend parameters.
void Actor03700_Fn034A0(Task* task)
{
    GsCOORDINATE2* coord;
    VECTOR         vec;

    coord  = ((TmdObject*)task->extra)->coords;
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    Gp_UpdateActorColor(task->spawnArg2, &vec, 0, 0);
}
