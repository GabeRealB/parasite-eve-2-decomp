#ifndef ACTORS_SHARED_80132860_H
#define ACTORS_SHARED_80132860_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"

/// Work block the actors sharing this body hang off the task's `Task::idMap`
/// slot (0x1C), which is not a `TaskIdMap` here. The carriers allocate blocks
/// of different sizes, so only the two fields this body reaches are described:
/// `target` is the world position the actor turns to face, and `turnCount`
/// counts how many times that facing has been rebuilt.
typedef struct ActorsShared80132860Work {
    /* 0x000 */ byte    pad_0[0x4B8];
    /* 0x4B8 */ VECTOR3 target;
    /* 0x4C4 */ byte    pad_4C4[0x36];
    /* 0x4FA */ u16     turnCount;
} ActorsShared80132860Work;

/// Overlay of `GsCOORDINATE2` at `TmdObject::field_8`. Offset 0x44 (libgs
/// `param`, and `super` at 0x48) holds the Euler angles this body writes and
/// then hands straight to `RotMatrix`, the same reuse `HyperCoord` and
/// `GpCoordExt` document for their own overlays.
typedef struct ActorsShared80132860Coord {
    /* 0x00 */ s32     flg;
    /* 0x04 */ MATRIX  coord;
    /* 0x24 */ MATRIX  workm;
    /* 0x44 */ SVECTOR rot;
} ActorsShared80132860Coord;
STATIC_ASSERT_SIZEOF(ActorsShared80132860Coord, 0x4C);

/// Turns the actor's root part to face `work->target`: normalises the offset
/// from the part's own translation, takes its yaw with `ratan2`, and rebuilds
/// the local matrix from that yaw alone. Clearing `flg` makes
/// `Gp_UpdateCoordTree` recompute the world matrix from it.
///
/// Shared verbatim by `actor_120400`, `actor_135600`, `actor_335800` and
/// `actor_350700`.
void ActorsShared80132860(Task* task);

#endif
