#ifndef ROOMS_SHARED_801802CC_H
#define ROOMS_SHARED_801802CC_H

#include "common.h"

#include "main/task.h"

/// Drives one spark burst. State 0 fires the burst's own effect and then
/// branches on the spawn argument: a non-zero one starts a stream of jittered
/// sparks (state 1, three fresh LCG draws per frame feeding the spawn offset),
/// a zero one starts an expanding pair of `Room_Draw02` rings whose radius
/// grows by 0x30 and whose brightness falls by 0x20 each frame (state 2).
/// Either way the task reaches state 3 after seven frames and hands its
/// `GpEffWork` block back. It also bails out early - releasing the block once
/// the fade has run far enough - whenever `Gp_State1C->field_4` says the room
/// is leaving.
void RoomsShared801802cc(Task* task);

#endif // ROOMS_SHARED_801802CC_H
