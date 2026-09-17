#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/gameplay.h"
#include "gameplay/3A34.h"

#include "actors/actor_150400.h"

void ActorsShared8013242c(Task* task);

/// Refreshes the model root's coordinate, feeds its world translation (raised
/// by 800 on y) to `func_800D7A9C`, then ticks the animation state and draws
/// the ground shadow.
void ActorsShared80131e24Sub1(GpEnemy* enemy, Task* task)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    VECTOR         pos;

    obj   = (TmdObject*)task->extra;
    coord = obj->field_8;
    Gp_UpdateCoord(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1] - 800;
    pos.vz = coord->workm.t[2];
    func_800D7A9C(obj, &pos, 0, 3);
    ActorsShared80132a1c(task);
    ActorsShared8013242c(task);
}
