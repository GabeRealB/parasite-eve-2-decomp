#ifndef ACTORS_SHARED_80133DE4_H
#define ACTORS_SHARED_80133DE4_H

#include "common.h"

#include "main/task.h"

/// Work block the actors sharing this body hang off the task's `work` slot
/// (0x1C), which is not a `TaskIdMap` here. The carriers allocate blocks of
/// different sizes, so only the one field this body reaches is described: the
/// stored pitch the walk below steps toward its request.
typedef struct ActorsShared80133de4Work {
    /* 0x000 */ byte pad_0[0xF00];
    /* 0xF00 */ s16  field_F00; // pitch the tracker walks toward its target, clamped to 0..0x500
    /* 0xF02 */ byte pad_F02[0x2];
} ActorsShared80133de4Work;
STATIC_ASSERT_SIZEOF(ActorsShared80133de4Work, 0xF04);

/// Walk the actor's stored pitch toward `arg1`, moving at most 0x10 per call
/// and clamping the request to 0..0x500, then push the new angle onto the
/// model: part 3 gets half the pitch and part 4 the whole of it, each
/// corrected by the X rotation already baked into that part's coordinate
/// matrix (`ratan2` of `m[1][2]` over `m[2][2]`), and both coordinates are
/// marked dirty afterwards.
void ActorsShared80133de4(Task* task, s16 arg1);

#endif
