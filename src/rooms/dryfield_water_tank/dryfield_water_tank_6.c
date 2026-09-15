#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/D4.h"

#include "main/task.h"

extern TaskDesc D_dryfield_water_tank_80184DF4;

/// Per-view halfword table, indexed 1-based by `Gp_GetViewIndex()`. The value
/// the room publishes as its `Gp_State1C::field_A` variant index.
extern u16 D_dryfield_water_tank_801868CC[];

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank_6", func_dryfield_water_tank_8017EBA0);

void func_dryfield_water_tank_8017EC38(u32 arg0)
{
    Task_SpawnFromTable(&D_dryfield_water_tank_80184DF4, arg0 & 0xFFFF, (s32)(arg0 >> 0x10), 0);
}

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank_6", func_dryfield_water_tank_8017EC6C);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank_6", func_dryfield_water_tank_8017ED30);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank_6", func_dryfield_water_tank_8017EDF4);

INCLUDE_ASM("rooms/nonmatchings/dryfield_water_tank/dryfield_water_tank_6", func_dryfield_water_tank_8017EFF4);

/// Publishes the variant index the current camera view maps to: reads the view
/// index back and stores `D_dryfield_water_tank_801868CC[view - 1]` into the
/// shared work block's `field_A`. Gameplay holds this address in its data
/// (0x80110614, pointing at the room overlay), and `dryfield_parking_lot` and
/// `dryfield_water_tower` carry the same body.
void func_dryfield_water_tank_8017F084(void)
{
    Gp_State1C->field_A = D_dryfield_water_tank_801868CC[(Gp_GetViewIndex() & 0xFF) - 1];
}
