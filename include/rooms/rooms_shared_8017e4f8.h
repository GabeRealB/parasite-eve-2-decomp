#ifndef ROOMS_SHARED_8017E4F8_H
#define ROOMS_SHARED_8017E4F8_H

#include "common.h"

#include "main/task.h"
#include "rooms/room.h"
#include "main/coord.h"

/// The halo's shade table, one row per palette selector.
extern RoomHaloShade RoomsShared8017e4f8Shades[];

/// The two draw helpers the halo task calls: the per-frame halo itself and the
/// fading afterglow. Each carrying room names its own pair at its own address,
/// and they need not be the same routine in every room - the shared object only
/// needs a name to relocate against.
void RoomsShared8017e4f8Halo(GpCoord* coord, s16 frame, u8* rgb);
void RoomsShared8017e4f8Fade(GpCoord* coord, s16 frame, u8* rgb);

/// The rooms' expanding halo: state 0 parks the effect frame on its anchor and
/// works the fade step out of the spawn argument, state 1 draws the halo (plus
/// a half-bright echo on odd ticks) while the level ramps up, and state 2 fades
/// it back out through the afterglow helper before releasing the work block.
/// Eleven rooms carry this body.
void RoomsShared8017e4f8(Task* task);

#endif // ROOMS_SHARED_8017E4F8_H
