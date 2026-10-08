#ifndef MAIN_PRIVATE_GPUEXT_H
#define MAIN_PRIVATE_GPUEXT_H

#include "types.h"

/// Returns 1 while GPU display output is enabled, or 0 while it is blanked.
///
/// Samples the GP1 status word once; the active-high blanking bit is inverted.
s32 gpuExtIsDisplayEnabled(void);

#endif // MAIN_PRIVATE_GPUEXT_H
