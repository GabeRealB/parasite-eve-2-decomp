#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"

#include "actors/actors_shared_8016a98c.h"

void ActorsShared8016a98c(Task* task)
{
    GpEnemy*                  enemy = (GpEnemy*)task->spawnArg2;
    ActorsShared8016a98cWork* work  = (ActorsShared8016a98cWork*)task->idMap;
    GsCOORDINATE2*            coord = ((TmdObject*)task->extra)->field_8;
    ActorsShared8016a98cWork* objWork;

    enemy->field_54 = 0;

    objWork = (ActorsShared8016a98cWork*)task->idMap;
    Gp_UnlinkObj(&objWork->obj_2AC);
    Gp_UnlinkObj(&objWork->obj_2CC);
    Gp_UnlinkObj(&objWork->obj_3AC);

    work->field_430 = 0x1000;
    work->matrix_0  = coord->coord;

    Gp_SetLightMode((GpObj4C*)task->spawnArg2, 1);

    work->field_412 = 0;
    work->field_420++;
}
