#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"

/// Record handler (opcode 0x0D) of two of the room's script tables: calls
/// `Gp_PulseState1C` and raises bit 0 of `Gp_StateC08.field_6`.
void func_dryfield_water_tower_8017F8B0(void)
{
    Gp_PulseState1C();
    Gp_StateC08.field_6 |= 1;
}
