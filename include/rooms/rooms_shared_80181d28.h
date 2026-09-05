#ifndef ROOMS_SHARED_80181D28_H
#define ROOMS_SHARED_80181D28_H

#include "common.h"

#include "main/task.h"

#include <psyq/libgpu.h>
#include <psyq/libgte.h>

/// Per-frame scratch the shared glow-sprite task builds at `G_SCRATCH_HEAD`:
/// `pos` is the task coordinate's translation, projected through `GsWSMATRIX`
/// into `sxy`, `otz` is the resulting depth and `half` the half-extent
/// (`0x6180 / otz`) the camera-facing quad is drawn at, so the sprite shrinks
/// with distance. The block is exactly the 0x14 bytes the task reserves off the
/// scratch head.
typedef struct RoomsShared80181d28Scratch {
    /* 0x00 */ s32     otz;
    /* 0x04 */ s32     half;
    /* 0x08 */ SVECTOR pos;
    /* 0x10 */ DVECTOR sxy;
} RoomsShared80181d28Scratch;
STATIC_ASSERT_SIZEOF(RoomsShared80181d28Scratch, 0x14);

/// The glow sprite: a camera-facing textured quad at the task coordinate's
/// origin, flickering between two grey levels on alternate frames and picking
/// its CLUT and texture column from `spawnArg1`. One-shot - the work block is
/// released once the quad is queued, so the room respawns it every frame. Two
/// rooms carry this body.
void RoomsShared80181d28(Task* task);

#endif // ROOMS_SHARED_80181D28_H
