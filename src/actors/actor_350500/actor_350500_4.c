#include "common.h"

#include "actors/actor_350500.h"

#include "gameplay/1BC.h"
#include "main/task.h"

/// Exit callback `func_actor_350500_801623CC` installs; tears the task down.
void func_actor_350500_8016245C(Task* arg0)
{
    Gp_EnemyTaskExit(arg0);
}
