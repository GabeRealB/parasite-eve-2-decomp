#include "common.h"

/// Record handler (opcode 0x0D) of the actor's script data: queues the
/// replacing load of overlay 0x82.
void func_actor_503500_80132B78(void)
{
    CdCmd_EnqueueReplaceOverlay82();
}
