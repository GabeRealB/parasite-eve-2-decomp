#include "common.h"

/// Room script callback: clear the queued CD command and restart the CD queue.
void Room_Util13(void)
{
    CdCmd_CancelReplaceAndActivate();
}
