#include "common.h"

#include "gameplay/3CD8.h"
#include "gameplay/gameplay.h"

/// Sets bit 0 of `Gp_StateC08.field_6` and pulses `Gp_State1C`.
///
/// Shared body, linked into every room overlay that uses it.
void Room_Util15(void)
{
    Gp_StateC08.field_6 |= 1;
    Gp_PulseState1C();
}
