#include "common.h"

/// Cutscene script callback: clears the queued CD command and restarts the CD
/// queue.
void func_dryfield_night_dilapidated_house_8017DAD0(void)
{
    CdCmd_CancelReplaceAndActivate();
}
