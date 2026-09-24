#include "common.h"

/// Cutscene script callback: queues the replacement of overlay 0x82.
void func_dryfield_night_dilapidated_house_8017DA70(void)
{
    CdCmd_EnqueueReplaceOverlay82();
}
