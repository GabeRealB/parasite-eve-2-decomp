#include "common.h"

#include "main/task.h"

extern TaskDesc D_shelter_b3_dumping_hole_8018AFBC;

/// Spawns the task described by `D_shelter_b3_dumping_hole_8018AFBC`.
void func_shelter_b3_dumping_hole_80181A18(void)
{
    Task_SpawnFromTable(&D_shelter_b3_dumping_hole_8018AFBC, 0, 0, 0);
}
