#include "common.h"

#include "main/fs.h"

/// Record handler (opcode 0x0D) of the actor's script data: queues the load
/// of overlay 0x81.
void func_actor_361100_8016293C(void)
{
    CdCmd_EnqueueOverlay81();
}
