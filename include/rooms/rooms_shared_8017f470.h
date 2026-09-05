#ifndef ROOMS_SHARED_8017F470_H
#define ROOMS_SHARED_8017F470_H

#include "common.h"

#include "main/task.h"

/// The room's pair of elevator-door tasks. Both carrying rooms hold the pair at
/// their own address, named there by the family's symbol maps, so the shared
/// object owns no data.
extern Task* RoomsShared8017f470Cars[];

/// Tells both elevator doors to run forwards, by setting each task's
/// `spawnArg1` to 1. Two rooms carry this body.
s32 RoomsShared8017f470(void);

#endif // ROOMS_SHARED_8017F470_H
