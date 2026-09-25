#include "common.h"

#include "main/fs.h"

/// Script callback: queues the replacement overlay load.
void func_actor_342000_8016447C(void)
{
    CdCmd_EnqueueReplaceOverlay82();
}
