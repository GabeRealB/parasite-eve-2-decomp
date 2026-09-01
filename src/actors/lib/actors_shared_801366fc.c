#include "common.h"

#include "actors/actors_shared_801366fc.h"

#include "gameplay/1BC.h"

void ActorsShared801366fc(Task* task)
{
    Gp_DestroyEnemy(task->spawnArg2, task);
}
