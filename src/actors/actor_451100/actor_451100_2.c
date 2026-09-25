#include "common.h"

#include "actors/actor_451100.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/gameplay.h"
#include "main/task.h"
#include "main/tmd.h"

/// State 1 of the `func_actor_451100_801322D4` dispatcher, run each frame:
/// refreshes the model root's world matrix, relights the model from a point
/// 0x320 above its translation, then runs the step routine and draws the
/// ground shadow.
void func_actor_451100_80132330(GpEnemy* enemy, Task* task)
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
    func_actor_451100_80131F84(task);
    func_actor_451100_8013280C(task);
}

void func_actor_451100_801323B4(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}
