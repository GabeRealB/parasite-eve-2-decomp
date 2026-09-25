#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/gameplay.h"
#include "gameplay/3A34.h"

#include "actors/actor_150400.h"

/// State-1 handler of the actor's task, run every frame: refreshes the model
/// root's coordinate, feeds its world translation (raised by 800 on y) to
/// `func_800D7A9C`, then runs the animation step body and draws the ground
/// shadow.
void func_actor_150400_80132434(GpEnemy* enemy, Task* task)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    VECTOR         pos;

    obj   = (TmdObject*)task->extra;
    coord = obj->coords;
    Gp_UpdateCoord(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1] - 800;
    pos.vz = coord->workm.t[2];
    func_800D7A9C(obj, &pos, 0, 3);
    func_actor_150400_80132228(task);
    func_actor_150400_801324E0(task);
}
