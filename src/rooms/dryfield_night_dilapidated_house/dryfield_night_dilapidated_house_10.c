#include "common.h"

#include "main/task.h"

extern TaskDesc D_dryfield_night_dilapidated_house_801872B4;

/// Cutscene script callback: spawns the first task of the room's two-entry
/// descriptor table, the one that starts the streamed sequence.
void func_dryfield_night_dilapidated_house_8017DAF0(void)
{
    Task_SpawnFromTable(&D_dryfield_night_dilapidated_house_801872B4, 0, 0, 0);
}
