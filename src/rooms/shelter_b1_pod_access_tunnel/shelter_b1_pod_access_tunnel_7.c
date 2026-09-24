#include "common.h"
#include "gameplay/1BC.h"
#include "main/fs.h"

/// Queues the load of overlay 0x81.
void func_shelter_b1_pod_access_tunnel_8017E3BC(void)
{
    CdCmd_EnqueueOverlay81();
}

/// Restores the stream random-number state.
void func_shelter_b1_pod_access_tunnel_8017E3DC(void)
{
    Gp_RestoreStreamRng();
}
