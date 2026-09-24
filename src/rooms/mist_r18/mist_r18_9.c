#include "common.h"

#include "main/task.h"
#include "rooms/mist_r18.h"

/// Spawn entry 3 of the room's task table.
void func_mist_r18_8017E824(void)
{
    Task_SpawnFromTable(&D_mist_r18_80184F04, 3, 0, 0);
}
