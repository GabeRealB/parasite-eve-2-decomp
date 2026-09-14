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

/// Work block `func_actor_511000_80132480` allocates (`Mem_Calloc(0x4D4, 0)`)
/// and parks in that task's `Task::idMap` slot. Distinct from
/// `Actor511000Work`: this variant's light/color pair sits at 0x484 / 0x4A4
/// and is republished onto model part 1, not the root coordinate.
typedef struct Actor511000Work2 {
    /* 0x000 */ byte   pad_0[0x484];
    /* 0x484 */ MATRIX light;
    /* 0x4A4 */ MATRIX color;
    /* 0x4C4 */ byte   pad_4C4[0x10];
} Actor511000Work2;
STATIC_ASSERT_SIZEOF(Actor511000Work2, 0x4D4);

#endif // ACTOR_511000_H
