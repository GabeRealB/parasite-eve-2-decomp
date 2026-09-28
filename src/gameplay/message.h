#ifndef GAMEPLAY_PRIVATE_MESSAGE_H
#define GAMEPLAY_PRIVATE_MESSAGE_H

#include "common.h"

/// The same 0x7DB record as `GpCmdArg`, as the event script runner sends it
/// and a receiver that answers it sees it: `key` is the id the runner looked
/// its target up by, and the halfword `GpCmdArg` reads as `command` is two
/// bytes here. The receiver sets `done` once it has carried the request out,
/// which the runner waits for; the runner sets `field_3` with each request.
///
/// It stays a type of its own rather than a union inside `GpCmdArg` because a
/// union at `command` changes how the code that builds a `GpCmdArg` in place
/// schedule its stores.
typedef struct GpCmdReply {
    u16 key;
    s8  done;
    s8  field_3;
} GpCmdReply;
STATIC_ASSERT_SIZEOF(GpCmdReply, 4);

#endif // GAMEPLAY_PRIVATE_MESSAGE_H
