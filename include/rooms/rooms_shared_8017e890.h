#ifndef ROOMS_SHARED_8017E890_H
#define ROOMS_SHARED_8017E890_H

#include "common.h"

#include "main/task.h"

/// The second ring this burst draws each frame, after the shared halo. Every
/// carrying room names its own routine at its own address.
void RoomsShared8017e890Draw(GsCOORDINATE2* coord, s16 frame);

/// The rooms' twin-ring burst: seeds two independent levels on the first tick,
/// then each frame draws the halo and its own ring at the growing radius while
/// a wider, dimmer echo fades out behind them, releasing the work block once
/// the main level runs down. Eleven rooms carry this body.
void RoomsShared8017e890(Task* task);

#endif // ROOMS_SHARED_8017E890_H
