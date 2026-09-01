#ifndef ACTORS_SHARED_80132F24_H
#define ACTORS_SHARED_80132F24_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"

/// Leading part of the work block the actors sharing this body hang off the
/// task's `Task::idMap` slot (0x1C), which is not a `TaskIdMap` here. Each
/// sharer's spawn routine `Mem_Calloc`s its own, larger block -- 0x4C4 for
/// `actor_443500` up to 0x540 for `actor_113100` -- but every one of them
/// keeps the light/colour matrix pair at the same 0x478 / 0x498, which is why
/// the body below is byte-identical across all six overlays. The size below is
/// therefore the prefix this body touches, not any sharer's allocation.
typedef struct ActorsShared80132f24Work {
    /* 0x000 */ byte   pad_0[0x478];
    /* 0x478 */ MATRIX light;
    /* 0x498 */ MATRIX color;
} ActorsShared80132f24Work;
STATIC_ASSERT_SIZEOF(ActorsShared80132f24Work, 0x4B8);

/// Republishes the work block's two matrices onto `TmdObject::field_1C` /
/// `field_20`, the light/colour pair `Gp_BindDefaultMtx` otherwise points at
/// `Gp_DefaultMtx` / `Gp_DefaultMtx2`, so the actor draws with its own
/// lighting. `func_actor_213100_8014A23C` and `func_actor_503500_801324EC` are
/// the same body over differently sized work blocks, so they cannot join this
/// object.
void ActorsShared80132f24(Task* task);

#endif
