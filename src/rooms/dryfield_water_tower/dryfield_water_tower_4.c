#include "common.h"

#include "main/task.h"

#include "rooms/dryfield_water_tower.h"

void func_dryfield_water_tower_8017F8E8(s16 arg0)
{
    DryfieldWaterTowerState* state = (DryfieldWaterTowerState*)D_dryfield_water_tower_801876A4->idMap;

    state->field_5C = arg0;
    state->field_5E = 0;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower_4", func_dryfield_water_tower_8017F908);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower_4", func_dryfield_water_tower_8017F9AC);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower_4", func_dryfield_water_tower_8017FA5C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower_4", func_dryfield_water_tower_8017FB4C);

void func_dryfield_water_tower_8017FBC8(Task* task)
{
    ((DryfieldWaterTowerState*)task->idMap)->field_6C = 1;
}

void func_dryfield_water_tower_8017FBD8(Task* task)
{
    ((DryfieldWaterTowerState*)task->idMap)->field_6E = 1;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower_4", func_dryfield_water_tower_8017FBE8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower_4", func_dryfield_water_tower_8017FD64);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower_4", func_dryfield_water_tower_8017FF5C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower_4", func_dryfield_water_tower_80180038);
