#ifndef ROOMS_SHARED_8017DB1C_H
#define ROOMS_SHARED_8017DB1C_H

#include "common.h"

#include "main/task.h"

/// The motel balcony's script-event hook: cues one of the two 0x521D balcony
/// sounds and reports "handled nothing" for every other event. The day and
/// night balcony both carry this body.
s32 RoomsShared8017db1c(Task* task, s32 msgId, s32 event, s32 arg3);

#endif // ROOMS_SHARED_8017DB1C_H
