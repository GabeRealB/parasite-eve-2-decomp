#include "common.h"

#include "actors/actor_146300.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/task.h"
#include "main/tmd.h"

/// Second state of `ActorsShared80131f9c`: refreshes the model root's world
/// matrix, relights the model from a point 0x320 above its translation, then
/// runs the overlay's own per-frame update.
void ActorsShared80131f9cSub1(GpEnemy* enemy, Task* task)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    VECTOR         vec;

    obj   = task->extra;
    coord = obj->coords;
    Gp_UpdateCoord(coord);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1] - 0x320;
    vec.vz = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    func_actor_146300_801327CC(task);
}
