#include "common.h"

#include "actors/actor_160700.h"
#include "actors/actors_shared_8014c874.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/gameplay.h"
#include "main/task.h"
#include "main/tmd.h"

void ActorsShared80132378(Task* task);

/// Refreshes the model root's coordinate, lifts its world translation by 800 on
/// y, and hands the result to the light solve against the model object itself.
void ActorsShared80131e24Sub1(GpEnemy* enemy, Task* task)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    VECTOR         vec;

    obj   = task->extra;
    coord = obj->field_8;
    Gp_UpdateCoord(coord);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1] - 800;
    vec.vz = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
    ActorsShared8014c874(task);
    ActorsShared80132378(task);
}
