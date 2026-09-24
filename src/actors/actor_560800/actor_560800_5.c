#include "common.h"

#include "actors/actor_560800.h"
#include "main/task.h"

/// Spawns entry 2 of the actor's task descriptor table with `arg0` as its
/// spawn argument. Script tables in the actor's data call it.
void func_actor_560800_80136280(s32 arg0)
{
    Task_SpawnFromTable(&D_actor_560800_801718F0, 2, arg0, 0);
}
