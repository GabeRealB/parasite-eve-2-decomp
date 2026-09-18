#ifndef ACTORS_SHARED_80137FB0_H
#define ACTORS_SHARED_80137FB0_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

#include "gameplay/3A34.h"

/// Display object hung off the context's 0x2C slot; `field_8` is the model's
/// per-part `GsCOORDINATE2` array, whose root part this body reads as the
/// `GpObj38` over the same address.
typedef struct ActorShared80137fb0Obj2C {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
} ActorShared80137fb0Obj2C;

/// Work block the carriers hang off their context's 0x1C slot (the task's
/// `Task::work`, which is not a `TaskIdMap` here) -- the same block
/// `Actor402200Work` describes, seen through the one field this body reaches.
///
/// `field_6EA` is a pending tint request: this body reads it once a frame and,
/// while it is 1 or 2, hands the display object the matching translate
/// (0, 0, 0x400 or a full 0xFFF) and clears it, so each request is consumed on
/// the frame it is raised.
typedef struct ActorShared80137fb0Work {
    /* 0x000 */ byte pad_0[0x6EA];
    /* 0x6EA */ s16  field_6EA;
} ActorShared80137fb0Work;
STATIC_ASSERT_SIZEOF(ActorShared80137fb0Work, 0x6EC);

/// Actor context handed to this body: `field_1C` is the work block above,
/// `field_20` the `GpEnemy` the spawner left in the task's `Task::spawnArg2`
/// slot, and `field_2C` the display object.
typedef struct ActorShared80137fb0 {
    /* 0x00 */ byte                      pad_0[0x1C];
    /* 0x1C */ ActorShared80137fb0Work*  field_1C;
    /* 0x20 */ struct _GpEnemy*          field_20;
    /* 0x24 */ byte                      pad_24[8];
    /* 0x2C */ ActorShared80137fb0Obj2C* field_2C;
} ActorShared80137fb0;

/// Tints the actor from the world position of its mesh root, then, while the
/// work block's 0x6EA state is 1 or 2, sets the display object's translate to
/// the matching shade and consumes the state.
///
/// Shared verbatim by `actor_402200` and `actor_403900`.
void ActorsShared80137fb0(ActorShared80137fb0* arg0);

#endif
