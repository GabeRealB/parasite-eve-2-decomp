#include "common.h"
#include "actors/actor_110300.h"
#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"
#include "main/task.h"
#include "main/tmd.h"

/// Entry of the helper task: parents the given task's model root to node 8 of
/// the actor's model.
void func_actor_110300_80131FF8(GpActorWork* arg0)
{
    GsCOORDINATE2* parent;
    GsCOORDINATE2* coord;

    parent     = D_actor_110300_8013A0A4->extra->coords;
    coord      = arg0->extra->coords;
    coord->flg = 0;
    coord->sub = parent + 8;
}

/// Step 1 of the `func_actor_110300_80131F9C` dispatcher: run the body the
/// actor's step selects, then refresh the model root as step 0 did by feeding
/// its world translation to `func_800D7A9C` (the light solve) against the
/// model object itself.
///
/// The body reaches the task through the second argument, so the incoming `$a1`
/// is copied into `$a0` (the first, unused, is the `GpEnemy*`): that copy is
/// what the first call's argument, and the `Task::extra` load feeding it, are
/// both read off.
void func_actor_110300_80132020(GpEnemy* enemy, Task* task)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    VECTOR         vec;

    obj   = task->extra;
    coord = obj->coords;
    func_actor_110300_801320C4((GpActorWork*)task);
    vec.vx = coord->workm.t[0];
    vec.vy = coord->workm.t[1];
    vec.vz = coord->workm.t[2];
    func_800D7A9C(obj, &vec, 0, 3);
}

/// Exit callback the step-0 handler installs: kills the helper task, then
/// destroys the actor.
void func_actor_110300_80132088(Task* arg0)
{
    taskKill(D_actor_110300_8013A0A8);
    Gp_DestroyEnemy(arg0->spawnArg2, arg0);
}
