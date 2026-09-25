#include "common.h"

#include "actors/actor_151000.h"
#include "gameplay/3A34.h"
#include "gameplay/gameplay.h"
#include "main/task.h"
#include "main/tmd.h"

/// State 1 of the enemy's task: refreshes the model root's coordinate, hands
/// `func_800D7A9C` the point 0x320 above it, then runs the runner and draws the
/// ground shadow.
void func_actor_151000_80132450(GpEnemy* enemy, Task* task)
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
    func_actor_151000_80132084(task);
    func_actor_151000_80132A38(task);
}
