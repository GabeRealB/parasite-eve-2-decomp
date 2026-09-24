#include "common.h"

#include "main/task.h"
#include "rooms/shelter_b3_dumping_hole.h"

extern TaskDesc D_shelter_b3_dumping_hole_8018B588;

/// Selects entry `arg0` through `func_shelter_b3_dumping_hole_80181E70` with a
/// fixed third argument of 0xD0, then spawns the task described by
/// `D_shelter_b3_dumping_hole_8018B588`, passing `arg2` to the spawn.
void func_shelter_b3_dumping_hole_801830F0(s16 arg0, s16 arg1, s16 arg2)
{
    func_shelter_b3_dumping_hole_80181E70(arg0, arg1, 0xD0);
    Task_SpawnFromTable(&D_shelter_b3_dumping_hole_8018B588, 0, arg2, 0);
}
