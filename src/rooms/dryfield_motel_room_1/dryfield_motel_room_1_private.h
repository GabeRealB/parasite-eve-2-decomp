#ifndef SRC_ROOMS_DRYFIELD_MOTEL_ROOM_1_DRYFIELD_MOTEL_ROOM_1_PRIVATE_H
#define SRC_ROOMS_DRYFIELD_MOTEL_ROOM_1_DRYFIELD_MOTEL_ROOM_1_PRIVATE_H

#include "main/task_types.h"

/// Bodyless task recipe for the one-time opening Bone Suckler encounter.
///
/// Spawn descriptor index 0 with two zero payload words. Priority 192 orders
/// its per-frame updates; the callback allocates and owns its event work.
/// Keep the room overlay and placed actors live until the task ends.
extern TaskDesc gDryfieldMotelRoom1OpeningEventTaskDesc;

#endif // SRC_ROOMS_DRYFIELD_MOTEL_ROOM_1_DRYFIELD_MOTEL_ROOM_1_PRIVATE_H
