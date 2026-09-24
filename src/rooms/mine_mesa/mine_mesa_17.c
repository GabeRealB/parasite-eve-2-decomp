#include "common.h"

/// Room script callback: clear the queued CD command and restart the CD queue.
void func_mine_mesa_8017E600(void)
{
    CdCmd_CancelReplaceAndActivate();
}
