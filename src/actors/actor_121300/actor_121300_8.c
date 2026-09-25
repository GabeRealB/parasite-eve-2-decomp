#include "common.h"

#include "actors/actor_121300.h"
#include "main/task.h"

/// Spawns entry 1 of the overlay's spawn table with `arg0` as its argument.
void func_actor_121300_801342D4(s32 arg0)
{
    Task_SpawnFromTable(&D_actor_121300_8013D390, 1, arg0, 0);
}
