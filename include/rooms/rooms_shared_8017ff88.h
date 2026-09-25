#ifndef ROOMS_SHARED_8017FF88_H
#define ROOMS_SHARED_8017FF88_H

#include "common.h"

#include "main/task.h"
#include "rooms/room.h"

/// The halo's shade table, one row per palette selector.
extern RoomHaloShade RoomsShared8017ff88Shades[];

/// Expanding halo using `Room_Draw04` and `Room_DrawBillboard`: state 0 parks
/// the effect frame on its anchor and works the fade step out of the spawn
/// argument, state 1 draws the ring (plus a half-bright echo on odd ticks)
/// while the level ramps up, and state 2 fades it back out through the
/// billboard helper before releasing the work block. Four rooms carry this
/// body.
void RoomsShared8017ff88(Task* task);

#endif // ROOMS_SHARED_8017FF88_H
