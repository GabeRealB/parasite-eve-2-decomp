#ifndef ROOMS_SHARED_8017F280_H
#define ROOMS_SHARED_8017F280_H

#include "common.h"

#include "main/task.h"

/// Two-state dispatcher of a room's prompt script task: builds the handler
/// pair on the stack and tails into the entry named by `Task::state`. Both
/// handlers are themselves shared bodies, so the pair costs no rodata and the
/// seven rooms carrying this dispatcher share one object.
void RoomsShared8017f280(Task* task);

#endif // ROOMS_SHARED_8017F280_H
