#ifndef ROOMS_SHARED_8017D8D0_H
#define ROOMS_SHARED_8017D8D0_H

#include "common.h"

#include "main/task.h"

/// The fourteen handlers this dispatcher runs, one table per carrying room.
extern TaskFuncTable14 RoomsShared8017d8d0States;

/// Fourteen-state dispatcher of a room's main cutscene task: the handler table
/// is copied onto the stack and the entry named by `Task::state` is called.
/// Two rooms carry this body.
void RoomsShared8017d8d0(Task* task);

#endif // ROOMS_SHARED_8017D8D0_H
