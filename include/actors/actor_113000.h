#ifndef ACTOR_113000_H
#define ACTOR_113000_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/task.h"

/// Work block this actor parks in `Task::idMap`. The per-frame state fields
/// (`0x474`..`0x4C8`) are the animation context its handlers walk; `light` /
/// `color` are the matrices the TMD object's `field_1C` / `field_20` are
/// republished from. Only that pair is spelled out here; the prefix fields are
/// unreferenced by the bodies matched so far.
typedef struct Actor113000Work {
    /* 0x000 */ byte   pad_0[0x480];
    /* 0x480 */ MATRIX light;
    /* 0x4A0 */ MATRIX color;
} Actor113000Work;

#endif // ACTOR_113000_H
