#include "common.h"

#include "main/task.h"
#include "rooms/dryfield_main_street.h"

extern TaskDesc D_dryfield_main_street_8018156C;

/// Spawns the task described at `D_dryfield_main_street_8018156C` and keeps it
/// in `D_dryfield_main_street_80185630`.

void func_dryfield_main_street_8017E320(void)
{
    D_dryfield_main_street_80185630 = Task_SpawnFromTable(&D_dryfield_main_street_8018156C, 1, 0, 0);
}
