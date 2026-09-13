#ifndef ACTORS_SHARED_80137E48_H
#define ACTORS_SHARED_80137E48_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

/// Display object hung off the context's 0x2C slot; `field_8` is the model's
/// per-part `GsCOORDINATE2` array, whose root part this body integrates. Same
/// shape as the other actor overlays' display objects.
typedef struct ActorShared80137e48Obj2C {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
} ActorShared80137e48Obj2C;

/// Work block the carriers hang off their context's 0x1C slot (the task's
/// `Task::idMap`, which is not a `TaskIdMap` here) -- the same block
/// `Actor402200Work` describes, seen through the fields this body reaches.
///
/// `field_664`-`field_66C` latch the root translation before it moves, so the
/// motion the body adds is recoverable from the work block. `field_6C8` is the
/// step length the X/Z components advance by, in 4096-per-unit fixed point:
/// both components scale the matching diagonal of the root matrix by it rather
/// than the frame's facing, so the step keeps the direction the model was
/// built with. `field_714` is a climb state -- below 2 the root also rises a
/// fixed 0x80 a frame, whatever the step length is.
typedef struct ActorShared80137e48Work {
    /* 0x000 */ byte pad_0[0x664];
    /* 0x664 */ s32  field_664; // previous root translation X
    /* 0x668 */ s32  field_668; // previous root translation Y
    /* 0x66C */ s32  field_66C; // previous root translation Z
    /* 0x670 */ byte pad_670[0x58];
    /* 0x6C8 */ s16  field_6C8; // step length, 4096 = one unit
    /* 0x6CA */ byte pad_6CA[0x4A];
    /* 0x714 */ s16  field_714; // climb state; below 2 adds a fixed rise
    /* 0x716 */ byte pad_716[6];
} ActorShared80137e48Work;
STATIC_ASSERT_SIZEOF(ActorShared80137e48Work, 0x71C);

/// Actor context handed to this body: `field_1C` is the work block above and
/// `field_2C` the display object. Same shape as the other actor overlays'
/// contexts.
typedef struct ActorShared80137e48 {
    /* 0x00 */ byte                      pad_0[0x1C];
    /* 0x1C */ ActorShared80137e48Work*  field_1C;
    /* 0x20 */ byte                      pad_20[0xC];
    /* 0x2C */ ActorShared80137e48Obj2C* field_2C;
} ActorShared80137e48;

/// Latches the root translation into the work block, then advances the root
/// along the model's own X/Z diagonals by the work block's step length, and,
/// while the climb state is below 2, lifts it a fixed 0x80. The root matrix is
/// left alone, so the walk is a translation only.
///
/// Shared verbatim by `actor_402200` and `actor_403900`.
void ActorsShared80137e48(ActorShared80137e48* arg0);

#endif
