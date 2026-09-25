#include "common.h"

#include "main/fs.h"

/// Opcode-0x0D callback in the actor's cutscene script: queues CD command 0x82
/// through `CdCmd_EnqueueReplaceOverlay82`.
void func_actor_303600_80162658(void)
{
    CdCmd_EnqueueReplaceOverlay82();
}
