#ifndef ROOMS_SHARED_8017D878_H
#define ROOMS_SHARED_8017D878_H

#include "common.h"

#include "main/task.h"

/// The room's own three-state jump table for this task family. Every carrying
/// room has one, at its own address in its leading rodata, so the shared object
/// reaches it by a name the family's symbol maps emit per overlay rather than
/// owning any data itself.
extern const TaskFuncTable3 RoomsShared8017d878Table;

/// Dispatches a room task through the room's own three-entry state table.
/// The table is copied onto the stack first, so the call goes through a local
/// copy rather than the rodata - 146 rooms drive a task this way, and only the
/// table differs.
void RoomsShared8017d878(Task* task);

#endif // ROOMS_SHARED_8017D878_H
