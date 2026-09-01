#include "common.h"

#include "actors/actors_shared_8014ca28.h"

#include "gameplay/1BC.h"

void ActorsShared8014ca28(Task* task)
{
    ActorShared8014ca28Work* work;
    GpEnemy*                 enemy;

    enemy = task->spawnArg2;
    work  = (ActorShared8014ca28Work*)task->idMap;

    enemy->field_54 = 0;
    Gp_UnlinkNode(&enemy->node);
    Gp_UnlinkObj(&work->field_FC);
    Gp_UnlinkObj(&work->field_134);
    Gp_UnlinkObj(&work->field_1B4);
    Gp_UnlinkObj(&work->field_1EC);
    Gp_EnemyTaskExit(task);
}
