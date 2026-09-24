#include "common.h"
#include "main/fs.h"

/// Cancels the queued overlay replacement and restarts the CD queue.
void func_shelter_b1_pod_access_tunnel_8017E3FC(void)
{
    CdCmd_CancelReplaceAndActivate();
}
