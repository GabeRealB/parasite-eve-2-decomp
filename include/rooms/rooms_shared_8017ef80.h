#ifndef ROOMS_SHARED_8017EF80_H
#define ROOMS_SHARED_8017EF80_H

#include "common.h"

#include "main/task.h"

/// Drives one expanding flash burst. State 0 sizes the burst from the spawn
/// argument - the ring brightness starts at 0 and its radius at 0x80, and both
/// step by `0x100 / spawnArg1` per frame, so a larger argument makes a slower,
/// finer expansion. State 1 spends that budget one frame at a time: a bright
/// `Room_Draw04` ring at the current radius, a half-brightness one at twice the
/// radius, and a `Room_Draw02` ring that closes in from 0x300 as the others
/// grow. When the countdown reaches zero it whites the screen out with
/// `Gp_DrawFadeQuad` and goes to state 2, which draws a `Room_DrawBillboard`
/// afterglow at three times the radius while fading by 0x10 a frame; below 0x11
/// it falls through to state 3 and hands the `GpEffWork` block back. Like the
/// other room burst tasks it also bails out early - releasing the block once
/// the fade has run far enough - whenever `Gp_State1C->field_4` says the room
/// is leaving.
void RoomsShared8017ef80(Task* task);

#endif // ROOMS_SHARED_8017EF80_H
