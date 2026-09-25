#include "common.h"

#include "actors/actor_451100.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/gameplay.h"
#include "main/task.h"
#include "main/tmd.h"

void func_actor_451100_80132CD4(Task* task);

/// Refreshes the model root's coordinate, feeds its world translation (raised
/// by 800 on y) to `func_800D7A9C`, then runs the animation starter and
/// `func_actor_451100_80132CD4`, which draws a ground quad.
void func_actor_451100_80132C28(GpEnemy* enemy, Task* task)
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
    func_actor_451100_80132A1C(task);
    func_actor_451100_80132CD4(task);
}

void func_actor_451100_80132CAC(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}

INCLUDE_ASM("actors/nonmatchings/actor_451100/actor_451100_6", func_actor_451100_80132CD4);
