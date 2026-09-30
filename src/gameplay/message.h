#ifndef GAMEPLAY_PRIVATE_MESSAGE_H
#define GAMEPLAY_PRIVATE_MESSAGE_H

#include "common.h"

/// The 0x7DB completion record sent to non-actor tasks: `key` is the id the
/// runner looked its target up by. The receiver sets `done` once it has carried
/// the request out, which the runner waits for; the runner sets `field_3` with
/// each request.
typedef struct GpCmdReply {
    u16 key;
    s8  done;
    s8  field_3;
} GpCmdReply;
STATIC_ASSERT_SIZEOF(GpCmdReply, 4);

#endif // GAMEPLAY_PRIVATE_MESSAGE_H
