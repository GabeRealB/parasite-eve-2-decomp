#include "common.h"

#include "actors/actor_535700.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/gameplay.h"
#include "main/task.h"
#include "main/tmd.h"

void func_actor_535700_80133020(Task* task);

/// State 1 of the second enemy's task: refreshes the model root's coordinate,
/// hands `func_800D7A9C` the point 0x320 above it, then runs the state machine
/// and draws the ground shadow.
void func_actor_535700_80132F74(GpEnemy* enemy, Task* task)
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
    func_actor_535700_80132D68(task);
    func_actor_535700_80133020(task);
}

void func_actor_535700_80132FF8(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}

INCLUDE_ASM("actors/nonmatchings/actor_535700/actor_535700_7", func_actor_535700_80133020);
