#ifndef ROOMS_SHARED_8017F2F8_H
#define ROOMS_SHARED_8017F2F8_H

#include "common.h"

#include "main/task.h"

/// Two-state dispatcher of a room's mirror task: state 0 sets the mirror up,
/// state 1 is its per-frame update. Six rooms carry this body.
void RoomsShared8017f2f8(Task* task);

#endif // ROOMS_SHARED_8017F2F8_H
