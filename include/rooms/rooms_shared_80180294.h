#ifndef ROOMS_SHARED_80180294_H
#define ROOMS_SHARED_80180294_H

#include "common.h"

#include "main/task.h"

/// The sixteen state handlers of the carrying room's cap script. Each room
/// names its own table at its own address; the shared dispatcher only needs a
/// name to relocate against, and the handlers are of course different routines
/// in every room.
extern const TaskFuncTable16 RoomsShared80180294States;

/// Runs the cap script's current state. The sixteen handlers are copied onto
/// the stack first, so the call goes through a local table rather than through
/// `.rodata`.
void RoomsShared80180294(Task* task);

#endif // ROOMS_SHARED_80180294_H
