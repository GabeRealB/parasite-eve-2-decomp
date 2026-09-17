#include "common.h"
#include "actors/actor_110300.h"
#include "gameplay/3A34.h"
#include "gameplay/3FB8.h"
#include "main/task.h"
#include "main/tmd.h"

void func_actor_110300_80131FF8(GpActorWork* arg0)
{
    GsCOORDINATE2* parent;
    GsCOORDINATE2* coord;

    parent     = D_actor_110300_8013A0A4->extra->field_8;
    coord      = arg0->extra->field_8;
    coord->flg = 0;
    coord->sub = parent + 8;
}

/// The step-3 handler of the shared two-state dispatcher: runs the actor's own
/// step body - the shared dispatcher `func_actor_110300_801320C4`, which reads
/// the work block from the global rather than from the task - and then hands
/// the model root coordinate's world translation to the per-frame light probe
/// `func_800D7A9C`.
///
/// The body reaches the task through the second argument, so the incoming `$a1`
/// is copied into `$a0` (the first, unused, is the `GpEnemy*`): that copy is
/// what the first call's argument, and the `Task::extra` load feeding it, are
/// both read off.
void ActorsShared80131f9cSub1(GpEnemy* enemy, Task* task)
{
    TmdObject*     obj;
    GsCOORDINATE2* coord;
    VECTOR         pos;

    obj   = (TmdObject*)task->extra;
    coord = obj->field_8;
    func_actor_110300_801320C4((GpActorWork*)task);
    pos.vx = coord->workm.t[0];
    pos.vy = coord->workm.t[1];
    pos.vz = coord->workm.t[2];
    func_800D7A9C(obj, &pos, 0, 3);
}

void func_actor_110300_80132088(Task* arg0)
{
    Task_Kill(D_actor_110300_8013A0A8);
    Gp_DestroyEnemy(arg0->spawnArg2, arg0);
}

/// Runs the body the actor's step selects and then leaves it in step 3, the
/// running state. Steps 1 and 2 each return through their own copy of the
/// advance; the two are identical, so jump.c cross-jumps them and only the
/// second survives.
///
/// `arg0` is handed the actor but the body ignores it: it reaches the work
/// block through the global, like every other function in the overlay.
void func_actor_110300_801320C4(GpActorWork* arg0)
{
    if (ActorsShared80131f9cWork->field_474 == 1) {
        ActorsShared80132208();
        ActorsShared80131f9cWork->field_474 = 3;
        return;
    }
    if (ActorsShared80131f9cWork->field_474 == 2) {
        ActorsShared80132180();
        ActorsShared80131f9cWork->field_474 = 3;
        return;
    }
    if (ActorsShared80131f9cWork->field_474 == 3) {
        ActorsShared80132138();
    }
}
