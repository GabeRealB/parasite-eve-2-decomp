#include "common.h"

#include "rooms/dryfield_water_tower.h"

void func_dryfield_water_tower_80180174(s16 arg0)
{
    DwtwWork* work = (DwtwWork*)D_dryfield_water_tower_801876AC->idMap;

    work->field_C = arg0;
    work->field_E = 0;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower_5", func_dryfield_water_tower_80180194);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower_5", func_dryfield_water_tower_80180220);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower_5", func_dryfield_water_tower_801802D8);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower_5", func_dryfield_water_tower_80180348);
