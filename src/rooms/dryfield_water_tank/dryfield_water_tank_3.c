#include "common.h"
#include "main/task.h"
extern TaskDesc D_dryfield_water_tank_80184DF4;

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank_3", func_dryfield_water_tank_8017E174);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank_3", func_dryfield_water_tank_8017E194);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank_3", func_dryfield_water_tank_8017E1B4);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank_3", func_dryfield_water_tank_8017E220);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank_3", func_dryfield_water_tank_8017E3C4);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank_3", func_dryfield_water_tank_8017E568);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank_3", func_dryfield_water_tank_8017E78C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank_3", func_dryfield_water_tank_8017E9F8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank_3", func_dryfield_water_tank_8017EB80);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank_3", func_dryfield_water_tank_8017EBA0);

void func_dryfield_water_tank_8017EC38(u32 arg0)
{
    Task_SpawnFromTable(&D_dryfield_water_tank_80184DF4, arg0 & 0xFFFF, (s32)(arg0 >> 0x10), 0);
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank_3", func_dryfield_water_tank_8017EC6C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank_3", func_dryfield_water_tank_8017ED30);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank_3", func_dryfield_water_tank_8017EDF4);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank_3", func_dryfield_water_tank_8017EFF4);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank_3", func_dryfield_water_tank_8017F084);
