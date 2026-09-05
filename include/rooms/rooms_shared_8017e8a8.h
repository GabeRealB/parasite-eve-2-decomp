#ifndef ROOMS_SHARED_8017E8A8_H
#define ROOMS_SHARED_8017E8A8_H

#include "common.h"

#include "main/task.h"

/// The room's cutscene task, or NULL while none is running. Both carrying
/// rooms hold the pointer at their own address, named there by the family's
/// symbol maps.
extern Task* RoomsShared8017e8a8Task;

/// Drives the room's cutscene task to a given state, doing nothing when no
/// such task is running. Two rooms carry this body.
void RoomsShared8017e8a8(s32 arg0);

#endif // ROOMS_SHARED_8017E8A8_H
