#include "common.h"

#include "main/task.h"

#include "rooms/dryfield_water_tank.h"

extern TaskDesc D_dryfield_water_tank_80184DF4;
extern Task*    D_dryfield_water_tank_80188D50;

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank_4", func_dryfield_water_tank_8017E3C4);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank_4", func_dryfield_water_tank_8017E568);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank_4", func_dryfield_water_tank_8017E78C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank_4", func_dryfield_water_tank_8017E9F8);

void func_dryfield_water_tank_8017EB80(s16 arg0)
{
    DwtWork* work = (DwtWork*)D_dryfield_water_tank_80188D50->idMap;

    work->field_4 = arg0;
    work->field_6 = 0;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank_4", func_dryfield_water_tank_8017EBA0);

void func_dryfield_water_tank_8017EC38(u32 arg0)
{
    Task_SpawnFromTable(&D_dryfield_water_tank_80184DF4, arg0 & 0xFFFF, (s32)(arg0 >> 0x10), 0);
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank_4", func_dryfield_water_tank_8017EC6C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank_4", func_dryfield_water_tank_8017ED30);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank_4", func_dryfield_water_tank_8017EDF4);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank_4", func_dryfield_water_tank_8017EFF4);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank_4", func_dryfield_water_tank_8017F084);
