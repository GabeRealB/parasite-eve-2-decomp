#ifndef ACTORS_SHARED_80134EB8_H
#define ACTORS_SHARED_80134EB8_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

/// Partial view of the actor's work block behind `field_1C`. `field_134` is
/// the halfword flag the state handlers toggle to pick a direction; the two
/// part coordinates below travel apart while it is set and back while it is
/// clear.
typedef struct ActorShared80134eb8Work {
    /* 0x000 */ byte pad_0[0x134];
    /* 0x134 */ s16  field_134;
} ActorShared80134eb8Work;

/// `field_8` is the actor's `GsCOORDINATE2` array: entry 2 and entry 3 are the
/// two part coordinates this body repositions.
typedef struct ActorShared80134eb8Obj2C {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
} ActorShared80134eb8Obj2C;

typedef struct ActorShared80134eb8 {
    /* 0x00 */ byte                      pad_0[0x1C];
    /* 0x1C */ ActorShared80134eb8Work*  field_1C;
    /* 0x20 */ byte                      pad_20[0xC];
    /* 0x2C */ ActorShared80134eb8Obj2C* field_2C;
} ActorShared80134eb8;

/// Resets the two part coordinates and walks their local Z either apart or
/// back together, depending on the work block's direction flag: entry 2 is
/// parked at -0x5F with its travel bounded below by 0, entry 3's by 0x1E, so
/// the pair never collapses. Both coordinates have their `flg` cleared.
///
/// Shared by `actor_102400` and `actor_202400`.
void ActorsShared80134eb8(ActorShared80134eb8* arg0);

#endif
