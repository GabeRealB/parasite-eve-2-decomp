#ifndef GAMEPLAY_MESSAGE_H
#define GAMEPLAY_MESSAGE_H

#include "common.h"

#include <psyq/libgte.h>

// The records carried as the payload of task messages. A sender passes a
// pointer to one alongside the message id, and every task that answers the
// message reads the same layout, whichever overlay it lives in. Event scripts
// keep these records as data and send them as they run.

/// A position and a set of Euler angles, the payload of the messages that put
/// a task somewhere. The player takes it to be placed or warped, and as the
/// point to walk to; the actors take it to be placed, and as the point to walk
/// to or turn towards, where some read only the position.
typedef struct GpPlaceArg {
    VECTOR  pos;
    SVECTOR rot;
} GpPlaceArg;
STATIC_ASSERT_SIZEOF(GpPlaceArg, 0x18);

#endif
