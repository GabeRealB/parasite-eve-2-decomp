#include "common.h"

#include "main/fs.h"

/// Opcode-0x0D callback in the actor's cutscene script: queues CD command 0x81
/// through `CdCmd_EnqueueOverlay81`.
void func_actor_303600_80162678(void)
{
    CdCmd_EnqueueOverlay81();
}
