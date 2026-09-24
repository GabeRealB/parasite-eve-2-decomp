#include "common.h"

#include "main/fs.h"

/// Script callback in the actor's data tables: queues the replacement of
/// overlay 0x82.
void func_actor_560800_80136910(void)
{
    CdCmd_EnqueueReplaceOverlay82();
}
