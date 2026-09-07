#include "common.h"

#include "main/tmd.h"

#include "gameplay/1BC.h"

#include "actors/actors_shared_801384ac.h"

/// Exit callback: unlink the four display nodes, relink the second part coord
/// under the model's root, then let gameplay tear the enemy down.
void ActorsShared801384ac(Task* task)
{
    ActorShared801384acWork* work;
    GpEnemy*                 enemy;
    GsCOORDINATE2*           coord;
    s32                      i;

    enemy = task->spawnArg2;
    work  = (ActorShared801384acWork*)task->idMap;
    for (i = 0; i < 4; i++) {
        Gp_UnlinkObj(&work->field_9A8[i]);
    }
    coord        = ((TmdObject*)task->extra)->field_8;
    coord[1].sub = coord;
    Gp_DestroyEnemy(enemy, task);
}
