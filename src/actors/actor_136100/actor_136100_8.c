#include "common.h"

#include "actors/actor_136100.h"
#include "main/task.h"

/// Starts the fade-in (entry 4 of the actor's task table).
void func_actor_136100_80134898(void)
{
    Task_SpawnFromTable(D_actor_136100_80140744, 4, 9, 0);
}
