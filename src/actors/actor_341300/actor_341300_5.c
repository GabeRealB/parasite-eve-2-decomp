#include "common.h"

#include "gameplay/1BC.h"

/// Record handler (opcode 0x0D) of the actor's script data: restores the
/// stream random-number state.
void func_actor_341300_801623DC(void)
{
    Gp_RestoreStreamRng();
}
