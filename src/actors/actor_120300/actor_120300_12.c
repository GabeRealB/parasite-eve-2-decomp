#include "common.h"

#include "actors/actor_120300.h"
#include "main/task.h"

/// Spawns the fade task (entry 4 of the actor's task table) at rate 9.
void func_actor_120300_80133EE4(void)
{
    Task_SpawnFromTable(D_actor_120300_80141B6C, 4, 9, 0);
}
