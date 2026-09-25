#include "common.h"

#include "main/fs.h"

/// Record handler (opcode 0x0D) of the actor's script data: queues the
/// replacing load of overlay 0x82.
void func_actor_341300_8016239C(void)
{
    CdCmd_EnqueueReplaceOverlay82();
}
