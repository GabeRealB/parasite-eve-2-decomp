#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"

/// Script command that calls `Gp_PulseState1C` and sets bit 0 of
/// `Gp_StateC08.field_6`.
void func_dryfield_dilapidated_house_8017EA7C(void)
{
    Gp_PulseState1C();
    Gp_StateC08.field_6 |= 1;
}
