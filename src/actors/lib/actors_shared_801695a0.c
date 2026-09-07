#include "common.h"

#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"
#include "gameplay/3A34.h"

#include "actors/actors_shared_801695a0.h"

void ActorsShared801695a0(Task* task)
{
    GsCOORDINATE2*            coord = ((TmdObject*)task->extra)->field_8;
    GpEnemy*                  enemy = (GpEnemy*)task->spawnArg2;
    ActorsShared801695a0Work* work  = (ActorsShared801695a0Work*)task->idMap;
    ActorsShared801695a0Work* objWork;

    enemy->field_54 = 0;

    objWork = (ActorsShared801695a0Work*)task->idMap;
    Gp_UnlinkObj(&objWork->obj_2AC);
    Gp_UnlinkObj(&objWork->obj_2CC);
    Gp_UnlinkObj(&objWork->obj_3AC);

    work->field_430 = 0x1000;
    work->matrix_0  = coord->coord;

    Gp_SetLightMode((GpObj4C*)task->spawnArg2, 1);

    work->field_412 = 0;
    work->field_420++;
}
