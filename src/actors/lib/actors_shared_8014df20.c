#include "common.h"

#include "actors/actors_shared_8014df20.h"

#include "gameplay/1BC.h"

void ActorsShared8014df20(Task* task)
{
    ActorShared8014df20Work* work;
    GpEnemy*                 enemy;

    enemy = task->spawnArg2;
    work  = (ActorShared8014df20Work*)task->idMap;

    enemy->field_54 = 0;
    Gp_UnlinkNode(&enemy->node);
    Gp_UnlinkObj(&work->field_14C);
    Gp_UnlinkObj(&work->field_FC);
    Gp_UnlinkObj(&work->field_184);
    Gp_EnemyTaskExit(task);
}
