#ifndef ACTORS_SHARED_80162540_H
#define ACTORS_SHARED_80162540_H

#include "common.h"

#include <psyq/libgte.h>

#include "main/task.h"

/// Work block the carriers hang off the task's `Task::idMap` slot (0x1C),
/// which is not a `TaskIdMap` here. Both carriers allocate the same 0x4C8
/// bytes -- `Actor350700Work` and `Actor350500Work` -- but each documents only
/// the fields its own code reaches, so the two this body touches are described
/// here: `target` is the world position the actor turns to face, `state` is the
/// index into the carrier's state-handler table, and `field_4C4` picks between
/// the aimed and the biased facing.
typedef struct ActorsShared80162540Work {
    /* 0x000 */ byte    pad_0[0x480];
    /* 0x480 */ VECTOR3 target;
    /* 0x48C */ byte    pad_48C[0x36];
    /* 0x4C2 */ u16     state;
    /* 0x4C4 */ s8      field_4C4;
    /* 0x4C5 */ byte    pad_4C5[0x3];
} ActorsShared80162540Work;

/// Overlay of the `GsCOORDINATE2` at `TmdObject::field_8`, the actor's root
/// part. Offset 0x44 (libgs `param`, `super` at 0x48) holds the Euler angles
/// this body writes and then hands straight to `RotMatrix`, the same reuse
/// `ActorsShared80132860Coord` and `ActorsShared80133a68Coord` document for
/// their own overlays.
typedef struct ActorsShared80162540Coord {
    /* 0x00 */ s32     flg;
    /* 0x04 */ MATRIX  coord;
    /* 0x24 */ MATRIX  workm;
    /* 0x44 */ SVECTOR rot;
} ActorsShared80162540Coord;
STATIC_ASSERT_SIZEOF(ActorsShared80162540Coord, 0x4C);

/// Turns the actor's root part towards `work->target`: normalises the offset
/// from the part's own translation, takes its yaw with `ratan2`, and rebuilds
/// the local matrix from that yaw alone, clearing `flg` so `Gp_UpdateCoordTree`
/// recomputes the world matrix. Bumping `state` moves the actor on to the next
/// state handler.
///
/// Shared verbatim by `actor_350500` and `actor_350700`.
void ActorsShared80162540(Task* task);

#endif
