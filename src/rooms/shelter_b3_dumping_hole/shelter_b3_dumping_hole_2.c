#include "common.h"
#include "main/task.h"
extern TaskDesc D_shelter_b3_dumping_hole_80189ADC;

s32 func_shelter_b3_dumping_hole_8017D870(void)
{
    Task_SpawnFromTable(&D_shelter_b3_dumping_hole_80189ADC, 0, 0, 0);
    return 0;
}

INCLUDE_ASM("rooms/nonmatchings/shelter_b3_dumping_hole/shelter_b3_dumping_hole_2", func_shelter_b3_dumping_hole_8017D8A0);
