#include "common.h"
#include "actors/actor_350700.h"
#include "main/task.h"
#include "main/tmd.h"
#include "gameplay/1BC.h"

void func_actor_350700_80163348(void)
{
}

INCLUDE_ASM("actors/nonmatchings/actor_350700/actor_350700_2", func_actor_350700_80163350);

void func_actor_350700_801633BC(Task* arg0)
{
    Gp_EnemyTaskExit(arg0);
}

INCLUDE_RODATA("actors/nonmatchings/actor_350700/actor_350700_2", ActorsShared801327f8Table);

INCLUDE_RODATA("actors/nonmatchings/actor_350700/actor_350700_2", ActorsShared80132920Offset);
