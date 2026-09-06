#ifndef ROOMS_SHARED_8017E92C_H
#define ROOMS_SHARED_8017E92C_H

#include "common.h"

#include "main/task.h"

/// The room's own shade setter, still one copy per carrying room.
void RoomsShared8017e144(s16 shade);

/// Fades the room back out eight levels a frame, driving both the room shade
/// and the screen overlay, and advances the task's state once it bottoms out.
/// Two rooms carry this body.
void RoomsShared8017e92c(Task* task);

#endif // ROOMS_SHARED_8017E92C_H
