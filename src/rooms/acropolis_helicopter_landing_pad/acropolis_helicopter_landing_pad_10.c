#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"

/// Pulses the gameplay state with `Gp_PulseState1C` and sets bit 0 of
/// `Gp_StateC08.field_6`.
void func_acropolis_helicopter_landing_pad_8017E724(void)
{
    Gp_PulseState1C();
    Gp_StateC08.field_6 |= 1;
}
