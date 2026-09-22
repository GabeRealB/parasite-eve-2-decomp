#include "common.h"

#include "actors/actor_535700.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "main/task.h"
#include "main/tmd.h"

/// Per-frame update of the enemy's model: refreshes the root coordinate, hands
/// `func_800D7A9C` the point 0x320 above it, then runs the overlay's runner and
/// draws the ground quad.
void ActorsShared80131f9cSub1(GpEnemy* enemy, Task* task)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    VECTOR         pos;

    obj   = (TmdObject*)task->extra;
    coord = obj->coords;
    Gp_UpdateCoord(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1] - 0x320;
    pos.vz = coord->workm.t[2];
    func_800D7A9C(obj, &pos, 0, 3);
    func_actor_535700_80132108(task);
    func_actor_535700_80132ABC(task);
}

void func_actor_535700_80132558(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}
