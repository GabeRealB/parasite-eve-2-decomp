#include "common.h"

#include "actors/actors_shared_801330ac.h"
#include "gameplay/1BC.h"
#include "gameplay/3A34.h"
#include "gameplay/gameplay.h"
#include "main/task.h"
#include "main/tmd.h"

void func_actor_535700_80133020(Task* task);

/// The actor's draw body: refreshes the model root's coordinate, lights the
/// model at its world translation raised by 800 on y, then runs the shared
/// step body and this overlay's own follow-up.
void ActorsShared80131e24Sub1(GpEnemy* enemy, Task* task)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    VECTOR         pos;

    obj   = (TmdObject*)task->extra;
    coord = obj->field_8;
    Gp_UpdateCoord(coord);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1] - 0x320;
    pos.vz = coord->workm.t[2];
    func_800D7A9C(obj, &pos, 0, 3);
    ActorsShared801330ac(task);
    func_actor_535700_80133020(task);
}

void func_actor_535700_80132FF8(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}

INCLUDE_ASM("actors/nonmatchings/actor_535700/actor_535700_7", func_actor_535700_80133020);
