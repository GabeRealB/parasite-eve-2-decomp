#ifndef ACTORS_SHARED_80135FDC_H
#define ACTORS_SHARED_80135FDC_H

#include "actors/actors_shared_80138efc.h"

/// Steps the model's yaw toward the bearing `ActorsShared801357f0` just
/// wrote, then either rolls the next state or waits out a squared-distance
/// check against actor slot 3.
///
/// The latch at `field_BA8` arms motion 4 once and loads the countdown from
/// the placement byte at 0xBBB: 0xA when that byte is 0x31, otherwise 0x3C.
/// `field_46` then moves toward `field_B90` by at most 0x10, is masked to 12
/// bits and becomes the Y rotation. A bearing inside ±0x7F draws state 0xC or
/// 0xD from the LCG. Otherwise the countdown runs, and at zero the distance
/// arms state 0xB for kind 0xB within 0x89543F and kind 0x31 within 0x22550F.
///
/// Carried by `actor_101100`, `actor_104900`, `actor_201100`, `actor_204900`
/// and `actor_301100`.
void ActorsShared80135fdc(GpEnemy* enemy, Task* task, ActorsShared80138efcWork* work);

#endif // ACTORS_SHARED_80135FDC_H
