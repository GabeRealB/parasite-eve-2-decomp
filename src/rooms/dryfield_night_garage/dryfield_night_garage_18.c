#include "common.h"

/// Cancels the queued overlay replacement and restarts the CD queue.
void func_dryfield_night_garage_80180984(void)
{
    CdCmd_CancelReplaceAndActivate();
}
