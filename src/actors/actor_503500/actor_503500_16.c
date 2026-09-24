#include "common.h"
#include "gameplay/3CD8.h"

/// Record handler (opcode 0x0D) of the actor's script data: calls
/// `Gp_PulseState1C`.
void func_actor_503500_80132CA4(void)
{
    Gp_PulseState1C();
}
