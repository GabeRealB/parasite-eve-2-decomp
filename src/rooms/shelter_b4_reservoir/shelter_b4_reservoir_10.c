#include "common.h"

#include "gameplay/3CD8.h"

/// Callback the room's event tables name: requests the 0x100 pulse from
/// `Gp_State1C`.
void func_shelter_b4_reservoir_8017E7A8(void)
{
    Gp_PulseState1C();
}
