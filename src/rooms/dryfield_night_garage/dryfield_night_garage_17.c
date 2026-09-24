#include "common.h"

#include "main/fs.h"

/// Queues the load of overlay 0x81.
void func_dryfield_night_garage_80180944(void)
{
    CdCmd_EnqueueOverlay81();
}

/// Restores the stream random-number state.
void func_dryfield_night_garage_80180964(void)
{
    Gp_RestoreStreamRng();
}
