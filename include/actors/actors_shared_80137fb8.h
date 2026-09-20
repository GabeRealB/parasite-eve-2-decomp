#ifndef ACTORS_SHARED_80137FB8_H
#define ACTORS_SHARED_80137FB8_H

#include "common.h"
#include "gameplay/3FB8.h"
#include "main/session.h"
#include "main/task.h"

/// 0x58-byte work block the body at 0x80137C88 allocates with
/// `memCalloc(0x58, 0)` and parks in `Task::work` (0x1C), which is not a
/// `TaskIdMap` here. The same block lies under the `Actor101100Work` of the four
/// sibling slots (`actor_101100`, `actor_201100`, `actor_204900`,
/// `actor_301100`), whose 0x28 run is the `GpActorD4Rec` filled in here: the
/// object's `ctx.d4rec` points at it and its `recs` at the one-entry `GpRec18`
/// collision table at 0x40, which is where the 0x58 bytes end.
typedef struct ActorsShared80137fb8Work {
    /// Effect velocity: the random direction vector rotated by the actor's
    /// coordinate and scaled by a 0x1000-fraction draw. Its X and Z are added to
    /// the coordinate's translation afterwards, its Y is not.
    /* 0x00 */ SVECTOR vel;
    /// Display node linked as kind 3 with a `Gp_PackPair` payload.
    /* 0x08 */ GpObj        obj;
    /* 0x28 */ GpActorD4Rec rec;
    /* 0x40 */ GpRec18      rec18[1];
} ActorsShared80137fb8Work;
STATIC_ASSERT_SIZEOF(ActorsShared80137fb8Work, 0x58);

/// Each carrier resolves this byte at its own actor-ID storage.
extern u8 ActorsShared80137fb8ActorId;

void ActorsShared80137fb8(Task* task);

#endif // ACTORS_SHARED_80137FB8_H
