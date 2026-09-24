#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"

/// Sets bit 0 of `Gp_StateC08.field_6` and pulses `Gp_State1C`.
void func_mine_cavern_8017E2FC(void)
{
    Gp_StateC08.field_6 |= 1;
    Gp_PulseState1C();
}
