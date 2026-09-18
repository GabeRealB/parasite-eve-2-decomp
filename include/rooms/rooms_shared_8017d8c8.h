#ifndef ROOMS_SHARED_8017D8C8_H
#define ROOMS_SHARED_8017D8C8_H

#include "common.h"

#include "gameplay/D4.h"
#include "main/task.h"

/// The room's own message table, the one this task publishes. Every carrying
/// room has its own copy at its own address, named there by the family's
/// symbol maps, so the shared object owns no data.
extern GpMsgEntry RoomsShared8017d8c8Msgs[];

/// State 0 of a room's message-driven task family, in the variant that also
/// arms the stage-3 `D_80062735` mode byte: park the room's message table in
/// `Task::msgTable`, publish the task in pointer slot 7, advance to state 1.
/// The mine tunnels open their event family this way.
void RoomsShared8017d8c8(Task* task);

#endif // ROOMS_SHARED_8017D8C8_H
