#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"

/// Sets bit 0 of `Gp_StateC08.field_6` and pulses `Gp_State1C`.
void func_shelter_b6_corridor_8017E204(void)
{
    Gp_StateC08.field_6 |= 1;
    Gp_PulseState1C();
}
