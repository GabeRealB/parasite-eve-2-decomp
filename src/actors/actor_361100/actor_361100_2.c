#include "common.h"

#include "actors/actor_361100.h"

#include "main/task.h"
#include "main/tmd.h"

#include "gameplay/1BC.h"

extern Task* D_actor_361100_80171BE0;

extern TaskDesc D_actor_361100_80165C58;

void func_actor_361100_8016299C(void)
{
    D_actor_361100_80171BE0 = Task_SpawnFromTable(&D_actor_361100_80165C58, 0, 0, 0);
}

INCLUDE_ASM("actors/nonmatchings/actor_361100/actor_361100_2", func_actor_361100_801629D0);

void func_actor_361100_80162A24(s32 arg0)
{
    Task_SpawnFromTable(&D_actor_361100_80165C58, 1, arg0, 0);
}

INCLUDE_ASM("actors/nonmatchings/actor_361100/actor_361100_2", func_actor_361100_80162A54);
