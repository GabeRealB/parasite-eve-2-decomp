#include "common.h"

/// Record handler (opcode 0x0D) of the actor's script data: restores the
/// stream random-number state.
void func_actor_503500_80132BB8(void)
{
    Gp_RestoreStreamRng();
}
