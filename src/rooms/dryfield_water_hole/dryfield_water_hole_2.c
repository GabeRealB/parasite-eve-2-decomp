#include "common.h"

#include "gameplay/D4.h"

#include "main/session.h"
#include "main/task.h"

extern GpMsgEntry D_dryfield_water_hole_8017FC5C[];
extern TaskDesc   D_dryfield_water_hole_8017FC8C[];

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_hole/dryfield_water_hole_2", func_dryfield_water_hole_8017D78C);

void func_dryfield_water_hole_8017D7DC(Task* arg0)
{
    arg0->field_24 = D_dryfield_water_hole_8017FC5C;
    Game_SetPtrSlot(arg0, 7);
    Task_SpawnFromTable(D_dryfield_water_hole_8017FC8C, 0, 0, 0);
    arg0->state = (s32)(arg0->state + 1);
}

void func_dryfield_water_hole_8017D838(void)
{
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_hole/dryfield_water_hole_2", func_dryfield_water_hole_8017D840);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_hole/dryfield_water_hole_2", func_dryfield_water_hole_8017D898);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_hole/dryfield_water_hole_2", func_dryfield_water_hole_8017DFA0);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_hole/dryfield_water_hole_2", func_dryfield_water_hole_8017E000);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_hole/dryfield_water_hole_2", func_dryfield_water_hole_8017E040);
