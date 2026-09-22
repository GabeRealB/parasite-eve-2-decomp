#include "common.h"

#include "actors/actors_shared_80132ecc.h"

#include "gameplay/1BC.h"

void ActorsShared80132ecc(Task* task)
{
    ActorsShared80132eccWork* work = (ActorsShared80132eccWork*)task->work;

    Gp_DestroyEnemy(task->spawnArg2, task);
    taskKill(work->field_4F0);
    taskKill(work->field_4F4);
}
