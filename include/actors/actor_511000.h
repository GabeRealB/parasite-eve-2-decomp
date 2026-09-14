#ifndef ACTOR_511000_H
#define ACTOR_511000_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "main/task.h"

/// Work block this actor parks in `Task::idMap`. `light` / `color` are the
/// matrices the TMD object's `field_1C` / `field_20` are republished from.
/// Only that pair is spelled out here; the prefix fields are unreferenced by
/// the bodies matched so far.
typedef struct Actor511000Work {
    /* 0x00 */ byte   pad_0[0x30];
    /* 0x30 */ MATRIX light;
    /* 0x50 */ MATRIX color;
} Actor511000Work;

#endif // ACTOR_511000_H
