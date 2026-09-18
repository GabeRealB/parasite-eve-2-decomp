#ifndef ACTORS_SHARED_8013806C_H
#define ACTORS_SHARED_8013806C_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

/// The actor's per-part attach coordinates, 0x50 apart, hanging off the
/// display object's 0x08 slot (`TmdObject::coords`, the trailing per-part
/// `GsCOORDINATE2` array). This body reaches the root and the fourth part, so
/// only those two are named.
typedef struct ActorShared8013806cCoord {
    /* 0x000 */ GsCOORDINATE2 field_0;
    /* 0x050 */ byte          pad_50[0xA0];
    /* 0x0F0 */ GsCOORDINATE2 field_F0;
} ActorShared8013806cCoord;
STATIC_ASSERT_SIZEOF(ActorShared8013806cCoord, 0x140);

typedef struct ActorShared8013806cObj2C {
    /* 0x00 */ byte                      pad_0[8];
    /* 0x08 */ ActorShared8013806cCoord* field_8;
} ActorShared8013806cObj2C;

/// Work block the carriers hang off their context's 0x1C slot (the task's
/// `Task::work`, which is not a `TaskIdMap` here) -- the same block
/// `Actor402200Work` describes, seen through the one field this body reaches.
///
/// `field_6E2` is the ground-shadow shade: `Gp_DrawEffGroundQuad` draws nothing
/// for a negative value, and the block is calloc'd, so this body turns the
/// initial zero into -1 the first time it runs. An actor that never raises the
/// shade therefore casts no shadow.
typedef struct ActorShared8013806cWork {
    /* 0x000 */ byte pad_0[0x6E2];
    /* 0x6E2 */ s16  field_6E2;
} ActorShared8013806cWork;

typedef struct ActorShared8013806c {
    /* 0x00 */ byte                      pad_0[0x1C];
    /* 0x1C */ ActorShared8013806cWork*  field_1C;
    /* 0x20 */ byte                      pad_20[0xC];
    /* 0x2C */ ActorShared8013806cObj2C* field_2C;
} ActorShared8013806c;

/// Draws the actor's ground shadow. The x/z come from the fourth attach
/// coordinate and the y from the root, so the quad lies flat on the ground
/// under the actor rather than following the tilt of either part.
///
/// Shared verbatim by `actor_402200` and `actor_403900`.
void ActorsShared8013806c(ActorShared8013806c* arg0);

#endif
