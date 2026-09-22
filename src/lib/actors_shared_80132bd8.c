#include "common.h"

/// Clear the queued CD command and restart the CD queue.
void ActorsShared80132bd8(void)
{
    CdCmd_CancelReplaceAndActivate();
}
