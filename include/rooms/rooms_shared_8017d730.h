#ifndef ROOMS_SHARED_8017D730_H
#define ROOMS_SHARED_8017D730_H

#include "common.h"

#include "main/task.h"

/// The room's own message table, the one this task publishes. Every carrying
/// room has its own copy at its own address, named there by the family's
/// symbol maps, so the shared object owns no data.
extern s32 RoomsShared8017d730Msgs;

/// State 0 of a room's message-driven task family, in the variant that also
/// raises the `D_80115598` flag: park the room's message table in
/// `Task::field_24`, publish the task in pointer slot 7, advance to state 1.
/// Five rooms open their event family this way.
void RoomsShared8017d730(Task* task);

#endif // ROOMS_SHARED_8017D730_H
