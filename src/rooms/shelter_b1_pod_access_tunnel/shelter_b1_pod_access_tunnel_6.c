#include "common.h"
#include "main/fs.h"

/// Queues the replacement of overlay 0x82.
void func_shelter_b1_pod_access_tunnel_8017E39C(void)
{
    CdCmd_EnqueueReplaceOverlay82();
}
