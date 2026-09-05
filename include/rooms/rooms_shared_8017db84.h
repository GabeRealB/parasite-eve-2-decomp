#ifndef ROOMS_SHARED_8017DB84_H
#define ROOMS_SHARED_8017DB84_H

#include "common.h"

#include "gameplay/D4.h"
#include "main/task.h"

/// The room's own `GpMsgEntry[]` - the message table its cap scripts index.
/// Every carrying room has one, at its own address, so the shared object
/// reaches it by a name the family's symbol maps emit per overlay rather than
/// owning any data itself.
extern GpMsgEntry RoomsShared8017db84Msgs[];

/// State 0 of a room's message-driven task family: park the room's message
/// table in `Task::field_24`, publish the task in pointer slot 7 so the cap
/// interpreter can find it, and advance to state 1. Fifty-one rooms open their
/// event family exactly this way; only the table differs.
void RoomsShared8017db84(Task* task);

#endif // ROOMS_SHARED_8017DB84_H
