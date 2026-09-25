#include "common.h"
#include "main/task.h"
#include "actors/actor_160900.h"

/// Spawns `func_actor_160900_801344D8`, entry 1 of `D_actor_160900_8013FB50`,
/// with `arg0` as its first spawn argument.
void func_actor_160900_801346B0(s32 arg0)
{
    Task_SpawnFromTable(&D_actor_160900_8013FB50, 1, arg0, 0);
}
