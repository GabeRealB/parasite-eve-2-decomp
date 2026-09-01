#include "common.h"

#include "actors/actors_shared_80138570.h"

#include "gameplay/1BC.h"

/// Task exit callback: flags the enemy's list node, takes it and the three
/// display nodes back off their lists, and kills the task.
void ActorsShared80138570(Task* task)
{
    ActorShared80138570Work* work;
    GpEnemy*                 enemy;

    enemy = task->spawnArg2;
    work  = (ActorShared80138570Work*)task->idMap;

    enemy->node.field_4 = 1;
    enemy->field_54     = 0;
    Gp_UnlinkNode(&enemy->node);
    Gp_UnlinkObj(&work->field_1DC);
    Gp_UnlinkObj(&work->field_22C);
    Gp_UnlinkObj(&work->field_2AC);
    Gp_EnemyTaskExit(task);
}
