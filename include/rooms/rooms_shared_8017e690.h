#ifndef ROOMS_SHARED_8017E690_H
#define ROOMS_SHARED_8017E690_H

#include "common.h"

#include "main/task.h"

/// Opening state of the water-room ambience family: clear whichever of the
/// session's two water-level counters the current display mode selects, then
/// advance to state 1. Eight water rooms open their task exactly this way.
void RoomsShared8017e690(Task* task);

#endif // ROOMS_SHARED_8017E690_H
