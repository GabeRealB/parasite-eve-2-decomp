#include "common.h"

#include "actors/actor_121300.h"
#include "main/task.h"

/// Spawns entry 2 of the overlay's spawn table with `arg0` as its argument.
void func_actor_121300_80134304(s32 arg0)
{
    Task_SpawnFromTable(&D_actor_121300_8013D390, 2, arg0, 0);
}
