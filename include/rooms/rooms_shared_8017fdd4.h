#ifndef ROOMS_SHARED_8017FDD4_H
#define ROOMS_SHARED_8017FDD4_H

#include "common.h"

#include "main/task.h"

/// The two draw helpers this effect calls: the ring drawn twice per frame while
/// the flash ramps up, and the afterglow drawn while it fades. Each carrying
/// room names its own pair at its own address, and they need not be the same
/// routine in every room - the shared object only needs a name to relocate
/// against.
void RoomsShared8017fdd4Ring(GsCOORDINATE2* coord, s16 frame, u8* rgb);
void RoomsShared8017fdd4Fade(GsCOORDINATE2* coord, s16 frame, u8* rgb);

/// The rooms' white flash: state 0 seeds the level and the step from the spawn
/// count, state 1 draws the ring at two scales plus a full-screen quad while the
/// level ramps to 0xFF, and state 2 fades the afterglow back out before
/// releasing the work block. Eleven rooms carry this body.
void RoomsShared8017fdd4(Task* task);

#endif // ROOMS_SHARED_8017FDD4_H
