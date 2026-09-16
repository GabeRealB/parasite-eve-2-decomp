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
/// object's `field_1C` / `field_20` are republished from.
typedef struct Actor213000Work {
    /* 0x000 */ byte   pad_0[0x474];
    /* 0x474 */ s8     field_474; // nonzero while the animation context is live
    /* 0x475 */ byte   pad_475[0x2];
    /* 0x477 */ s8     field_477; // counts down to the buffer teardown
    /* 0x478 */ byte   pad_478[0x4];
    /* 0x47C */ MATRIX light;
    /* 0x49C */ MATRIX color;
} Actor213000Work;

#endif // ACTOR_213000_H
