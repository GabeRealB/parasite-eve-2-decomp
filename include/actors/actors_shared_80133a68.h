#ifndef ACTORS_SHARED_80133A68_H
#define ACTORS_SHARED_80133A68_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"

/// Work block the actors sharing this body hang off the task's `Task::work`
/// slot (0x1C), which is not a `TaskIdMap` here. The carriers allocate blocks
/// of different sizes, so only the two fields this body reaches are described:
/// `target` is the world position the actor turns to face, and `state` is the
/// index into the carrier's state-handler table, which every carrier's
/// dispatcher reads back through a sign-extending load.
typedef struct ActorsShared80133a68Work {
    /* 0x000 */ byte    pad_0[0x480];
    /* 0x480 */ VECTOR3 target;
    /* 0x48C */ byte    pad_48C[0x36];
    /* 0x4C2 */ u16     state;
} ActorsShared80133a68Work;

/// Overlay of the `GsCOORDINATE2` at `TmdObject::coords`, the actor's root
/// part. Offset 0x44 (libgs `param`, `super` at 0x48) holds the Euler angles
/// this body writes and then hands straight to `RotMatrix`, the same reuse
/// `ActorsShared80132860Coord` and `HyperCoord` document for their own overlays.
typedef struct ActorsShared80133a68Coord {
    /* 0x00 */ s32     flg;
    /* 0x04 */ MATRIX  coord;
    /* 0x24 */ MATRIX  workm;
    /* 0x44 */ SVECTOR rot;
} ActorsShared80133a68Coord;
STATIC_ASSERT_SIZEOF(ActorsShared80133a68Coord, 0x4C);

/// Turns the actor's root part to face `work->target`: normalises the offset
/// from the part's own translation, takes its yaw with `ratan2`, and rebuilds
/// the local matrix from that yaw alone. Clearing `flg` makes
/// `_gpUpdateCoordTree` recompute the world matrix from it, and bumping
/// `state` moves the actor on to the next state handler.
///
/// Shared verbatim by `actor_141000` and `actor_335800`.
void ActorsShared80133a68(Task* task);

#endif
