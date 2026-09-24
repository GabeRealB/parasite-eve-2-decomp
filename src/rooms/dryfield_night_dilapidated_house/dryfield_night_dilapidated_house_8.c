#include "common.h"

#include "main/fs.h"

/// Cutscene script callback: queues overlay 0x81.
void func_dryfield_night_dilapidated_house_8017DA90(void)
{
    CdCmd_EnqueueOverlay81();
}

/// Cutscene script callback: restores the stream random state.
void func_dryfield_night_dilapidated_house_8017DAB0(void)
{
    Gp_RestoreStreamRng();
}
