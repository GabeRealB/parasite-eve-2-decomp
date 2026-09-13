#ifndef ACTORS_SHARED_80135048_H
#define ACTORS_SHARED_80135048_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

/// The actor's attach object behind `field_2C`; `field_8` is the coordinate the
/// ground shadow is read from.
typedef struct ActorShared80135048Obj2C {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
} ActorShared80135048Obj2C;

typedef struct ActorShared80135048 {
    /* 0x00 */ byte                      pad_0[0x2C];
    /* 0x2C */ ActorShared80135048Obj2C* field_2C;
} ActorShared80135048;

/// Draws the actor's ground shadow: a quad at the attach coordinate's
/// translation, so it sits under the actor wherever the coordinate has been
/// placed this frame.
///
/// Shared verbatim by `actor_102400` and `actor_202400`.
void ActorsShared80135048(ActorShared80135048* arg0);

#endif
