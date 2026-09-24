#include "common.h"

#include "actors/actors_shared_80138570.h"

#include "gameplay/1BC.h"

/// Exit callback of the specimen's second form: flags the enemy's node, takes
/// it and the three render nodes back off their lists and runs the common
/// enemy task exit.
void Actor07000_Fn06750(Task* task)
{
    ActorShared80138570Work* work;
    GpEnemy*                 enemy;

    enemy = task->spawnArg2;
    work  = (ActorShared80138570Work*)task->work;

    enemy->node.flags = 1;
    enemy->recs       = 0;
    Gp_UnlinkNode(&enemy->node);
    Gp_UnlinkObj(&work->field_1DC);
    Gp_UnlinkObj(&work->field_22C);
    Gp_UnlinkObj(&work->field_2AC);
    Gp_EnemyTaskExit(task);
}
