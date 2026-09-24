#include "common.h"

#include "main/fs.h"

/// Clear the queued CD command and restart the CD queue.
void func_mist_r18_8017EC78(void)
{
    CdCmd_CancelReplaceAndActivate();
}
