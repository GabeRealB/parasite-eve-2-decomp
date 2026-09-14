#ifndef ACTOR_213000_H
#define ACTOR_213000_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/task.h"

/// Work block this actor parks in `Task::idMap`. `field_474` / `field_477`
/// drive the animation teardown, the animation context `Gp_AnimTickIndex`
/// walks starts the block, and `light` / `color` are the matrices the TMD
/// object's `field_1C` / `field_20` are republished from. Only that pair is
/// spelled out here; the prefix fields are unreferenced by the bodies that
/// write them.
typedef struct Actor213000Work {
    /* 0x000 */ byte   pad_0[0x47C];
    /* 0x47C */ MATRIX light;
    /* 0x49C */ MATRIX color;
} Actor213000Work;

#endif // ACTOR_213000_H
