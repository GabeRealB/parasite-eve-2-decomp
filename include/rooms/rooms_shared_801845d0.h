#ifndef ROOMS_SHARED_801845D0_H
#define ROOMS_SHARED_801845D0_H

#include "common.h"

/// Retunes the room's `RoomsShared8018459c` task: phases 0 and 1 are written
/// straight into its `spawnArg1`, anything else kills the task and clears the
/// handle. Two rooms carry this body.
void RoomsShared801845d0(s32 phase);

#endif // ROOMS_SHARED_801845D0_H
