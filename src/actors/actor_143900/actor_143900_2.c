#include "common.h"

#include "actors/actor_143900.h"
#include "gameplay/3A34.h"
#include "gameplay/gameplay.h"
#include "main/task.h"
#include "main/tmd.h"

/// Second state of the first variant's task: refreshes the model root's world
/// matrix, relights the model from a point 0x320 above its translation, then
/// runs the per-frame update and draws the ground shadow.
void func_actor_143900_80132380(GpEnemy* enemy, Task* task)
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
    func_actor_143900_80131FD4(task);
    func_actor_143900_8013242C(task);
}
