#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"

#include "actors/actor_341700.h"

/// Starts the death shrink, the same body as `func_actor_341700_801695A0`:
/// detaches the records, unlinks the three hit bodies, sets the Y scale to
/// 1.0, saves the root matrix, sets light mode 1 and advances the state.
void func_actor_341700_8016A98C(Task* task)
{
    GpEnemy*         enemy = (GpEnemy*)task->spawnArg2;
    Actor341700Work* work  = (Actor341700Work*)task->work;
    GsCOORDINATE2*   coord = ((TmdObject*)task->extra)->coords;
    Actor341700Work* objWork;

    enemy->recs = 0;

    objWork = (Actor341700Work*)task->work;
    Gp_UnlinkObj(&objWork->obj_2AC);
    Gp_UnlinkObj(&objWork->obj_2CC);
    Gp_UnlinkObj(&objWork->obj_3AC);

    work->field_430    = 0x1000;
    work->savedRootMtx = coord->coord;

    Gp_SetLightMode((GpObj4C*)task->spawnArg2, 1);

    work->field_412 = 0;
    work->field_420++;
}
