#ifndef ACTORS_SHARED_8013BE64_H
#define ACTORS_SHARED_8013BE64_H

#include "common.h"

#include "main/task.h"

/// Message 0x7D5 handler: the visibility switch an event task drives the actor
/// with. `arg2` is the sub-command and only 0 is distinguished from every other
/// value -- 0 writes the 0x80 flag into the host model's flag word and anything
/// else clears it. The second argument is the opcode id itself, which this
/// handler has no use for; `arg2` is the third slot of `GpMsgHandler`.
///
/// Shared verbatim by `actor_510900` and `actor_205200`.
s32 ActorsShared8013be64(Task* task, s32 msgId, s32 arg2);

#endif
