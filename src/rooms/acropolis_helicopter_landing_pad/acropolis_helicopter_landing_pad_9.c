#include "common.h"

#include "main/task.h"
#include "rooms/acropolis_helicopter_landing_pad.h"

/// Spawns entry 3 of the room's task table.
void func_acropolis_helicopter_landing_pad_8017E5B8(void)
{
    Task_SpawnFromTable(D_acropolis_helicopter_landing_pad_80184DA0, 3, 0, 0);
}
