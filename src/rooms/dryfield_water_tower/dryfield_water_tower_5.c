#include "common.h"
#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "rooms/dryfield_water_tower.h"

extern u16 D_dryfield_water_tower_801827A0[];

void func_dryfield_water_tower_80180174(s16 arg0)
{
    DwtwWork* work = (DwtwWork*)D_dryfield_water_tower_801876AC->idMap;

    work->field_C = arg0;
    work->field_E = 0;
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower_5", func_dryfield_water_tower_80180194);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower_5", func_dryfield_water_tower_80180220);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tower/dryfield_water_tower_5", func_dryfield_water_tower_801802D8);

void func_dryfield_water_tower_80180348(void)
{
    Gp_State1C->field_A = D_dryfield_water_tower_801827A0[(Gp_GetViewIndex() & 0xFF) - 1];
}
