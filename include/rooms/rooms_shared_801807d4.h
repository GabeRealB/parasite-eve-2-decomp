#ifndef ROOMS_SHARED_801807D4_H
#define ROOMS_SHARED_801807D4_H

#include "common.h"

#include "main/task.h"

/// The room's tracked animation task. Each carrying room holds the pointer at
/// its own address, named there by the family's symbol maps, so the shared
/// object owns no data.
extern Task* RoomsShared801807d4Task;

/// Drives the tracked task: an argument in 0..1 becomes its `spawnArg1`,
/// anything else kills the task and clears the pointer. Two rooms carry this
/// body; two more hold two copies each and keep their own.
void RoomsShared801807d4(s32 arg0);

#endif // ROOMS_SHARED_801807D4_H
