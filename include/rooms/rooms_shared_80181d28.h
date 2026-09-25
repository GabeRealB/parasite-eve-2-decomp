#ifndef ROOMS_SHARED_80181D28_H
#define ROOMS_SHARED_80181D28_H

#include "common.h"

#include "main/task.h"

#include <psyq/libgpu.h>
#include <psyq/libgte.h>

/// The glow sprite: a camera-facing textured quad at the task coordinate's
/// origin, flickering between two grey levels on alternate frames and picking
/// its CLUT and texture column from `spawnArg1`. One-shot - the work block is
/// released once the quad is queued, so the room respawns it every frame. Two
/// rooms carry this body.
void RoomsShared80181d28(Task* task);

#endif // ROOMS_SHARED_80181D28_H
