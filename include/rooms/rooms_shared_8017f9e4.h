#ifndef ROOMS_SHARED_8017F9E4_H
#define ROOMS_SHARED_8017F9E4_H

#include "common.h"

#include <psyq/libgs.h>

#include "main/task.h"

/// The two spawn offsets the shared smoke-trail task reads, aliased per overlay
/// in `configs/USA/sym/rooms/<overlay>.txt`. `[0]` seeds the object's own
/// coordinate on the first frame and `[1]` the second ring of trail
/// coordinates; `RoomsShared8017f9e4Pos2` is `[1]` under its own name, because
/// the per-frame path in state 1 rebuilds its address from scratch.
extern SVECTOR RoomsShared8017f9e4Pos[];
extern SVECTOR RoomsShared8017f9e4Pos2;

/// Drives a room's twin smoke trail. State 0 allocates a 0x500-byte block of
/// 16 `GsCOORDINATE2`s - eight for each trail - hangs it off the task's id-map
/// slot and seeds every one of them from the two spawn offsets, so the trail
/// starts collapsed on its origin. State 1 advances one coordinate of each
/// trail per frame (`field_22 & 7`, so the ring wraps every eight frames),
/// re-derives all sixteen against the view matrix and hands both halves to
/// `Room_Draw03`. The task frees itself once `field_22` reaches the spawn
/// argument. It idles whole while `Gp_State1C->field_4` says the room is
/// paused.
void RoomsShared8017f9e4(Task* task);

#endif // ROOMS_SHARED_8017F9E4_H
