#ifndef ROOMS_SHARED_80182628_H
#define ROOMS_SHARED_80182628_H

#include "common.h"

#include "main/task.h"

/// A one-shot sound cue on a timer: fires its spawn argument as a sound event
/// on the first frame, repeats it at frame 0x50 and kills itself at 0x78.
/// Seventeen rooms carry this body.
void RoomsShared80182628(Task* task);

#endif // ROOMS_SHARED_80182628_H
