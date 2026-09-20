#include "common.h"
#include "actors/actor_420700.h"
#include "main/task.h"
#include "main/tmd.h"

void func_actor_420700_8013239C(Task* arg0)
{
    taskKill(D_actor_420700_8013EFE8);
    Gp_DestroyEnemy(arg0->spawnArg2, arg0);
}

/// State handler of the model task: the spawn tick clears its root coordinate's
/// flag and the model's `field_C` and hangs the root off frame 4 of the actor's
/// own model, stepping to state 1; every later tick hands that actor model's
/// root translation, dropped by 0x320 in y, to `func_800D7A9C` for the part
/// colour matrix.
void func_actor_420700_801323D8(Task* task)
{
    TmdObject*     extra = task->extra;
    GsCOORDINATE2* coord = extra->coords;
    GsCOORDINATE2* parts = ((TmdObject*)D_actor_420700_8013EFE4->extra)->coords;
    GsCOORDINATE2* part  = parts + 4;
    VECTOR         vec;

    switch (task->state) {
        case 0:
            coord->flg   = 0;
            extra->flags = 0;
            coord->sub   = part;
            task->state++;
            break;
        case 1:
            vec.vx = parts->workm.t[0];
            vec.vy = parts->workm.t[1] - 0x320;
            vec.vz = parts->workm.t[2];
            func_800D7A9C(extra, &vec, 0, 3);
            break;
    }
}

/// Runs the body the actor's step selects and then leaves it in step 3, the
/// running state. Steps 1 and 2 each return through their own copy of the
/// advance; the two are identical, so jump.c cross-jumps them and only the
/// second survives.
void func_actor_420700_80132478(Task* task)
{
    if (ActorsShared80131f9cWork->field_4B4 == 1) {
        func_actor_420700_801325C8();
        ActorsShared80131f9cWork->field_4B4 = 3;
        return;
    }
    if (ActorsShared80131f9cWork->field_4B4 == 2) {
        ActorsShared80132538();
        ActorsShared80131f9cWork->field_4B4 = 3;
        return;
    }
    if (ActorsShared80131f9cWork->field_4B4 == 3) {
        func_actor_420700_801324EC();
    }
}

INCLUDE_ASM("actors/nonmatchings/actor_420700/actor_420700_2", func_actor_420700_801324EC);
