#ifndef ACTORS_SHARED_80132920_H
#define ACTORS_SHARED_80132920_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"

/// Work block the actors sharing this body hang off the task's `Task::idMap`
/// slot (0x1C), which is not a `TaskIdMap` here -- the same block
/// `ActorsShared80132860Work` describes, seen through the three fields this
/// body reaches. `step` is the per-frame translation the carrier's update loop
/// accumulates into the running offset at 0x4D8, `limit` is the per-axis
/// threshold that update compares the remaining delta against, and `turnCount`
/// is the counter `ActorsShared80132860` also bumps.
typedef struct ActorsShared80132920Work {
    /* 0x000 */ byte    pad_0[0x4C8];
    /* 0x4C8 */ VECTOR3 step;
    /* 0x4D4 */ byte    pad_4D4[0x14];
    /* 0x4E8 */ SVECTOR limit;
    /* 0x4F0 */ byte    pad_4F0[0xA];
    /* 0x4FA */ u16     turnCount;
} ActorsShared80132920Work;

/// The constant local-space offset this body rotates, `{ 0, 0, 0x200000, 0 }`
/// in every carrier: straight ahead along the part's own +Z. Each overlay
/// keeps its own copy in `.rodata`, so the address comes from the per-overlay
/// symbol map.
extern VECTOR ActorsShared80132920Offset;

/// Rotates `ActorsShared80132920Offset` through the root part's matrix and
/// parks the world-space result in `work->step`, so the carrier's update loop
/// walks the actor forwards. `limit` is opened to 0x7FFF on all three axes,
/// which disables the per-axis stop test, and `turnCount` records the move.
///
/// Shared verbatim by `actor_120400`, `actor_135600`, `actor_335800` and
/// `actor_350700`.
void ActorsShared80132920(Task* task);

#endif
