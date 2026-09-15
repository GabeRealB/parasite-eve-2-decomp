#include "common.h"

#include "main/task.h"

#include "rooms/dryfield_water_tower.h"

/// Scratch state of the room's cap script, stored at `Task::idMap`.
typedef struct {
    /* 0x00 */ u8  pad_0[0x5C];
    /* 0x5C */ s16 field_5C;
    /* 0x5E */ s16 field_5E;
    /* 0x60 */ u8  pad_60[0xC];
    /* 0x6C */ s16 field_6C;
    /* 0x6E */ s16 field_6E;
} DryfieldWaterTowerState;

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
