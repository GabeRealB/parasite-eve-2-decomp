#ifndef ACTORS_SHARED_80134F60_H
#define ACTORS_SHARED_80134F60_H

#include "common.h"
#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

/// Partial view of the actor's work block behind `field_1C`: the three
/// halfwords this body refreshes from the attach coordinate each frame, and
/// the speed at +0x138 that scales the axis it advances along.
typedef struct ActorShared80134f60Work {
    /* 0x000 */ byte pad_0[0x120];
    /* 0x120 */ s16  field_120;
    /* 0x122 */ s16  field_122;
    /* 0x124 */ s16  field_124;
    /* 0x126 */ byte pad_126[0x12];
    /* 0x138 */ s16  field_138;
} ActorShared80134f60Work;

typedef struct ActorShared80134f60Obj2C {
    /* 0x00 */ byte           pad_0[8];
    /* 0x08 */ GsCOORDINATE2* field_8;
} ActorShared80134f60Obj2C;

typedef struct ActorShared80134f60 {
    /* 0x00 */ byte                      pad_0[0x1C];
    /* 0x1C */ ActorShared80134f60Work*  field_1C;
    /* 0x20 */ byte                      pad_20[0xC];
    /* 0x2C */ ActorShared80134f60Obj2C* field_2C;
} ActorShared80134f60;

/// Saves the attach coordinate's world translation into the work block, then
/// advances it: Y by a fixed 0x80, and X and Z by the coordinate's own local Z
/// axis (`m[0][2]`, `m[2][2]`) scaled by `field_138` in 12-bit fixed point - a
/// step forward along the actor's facing.
///
/// Shared by `actor_102400` and `actor_202400`.
void ActorsShared80134f60(ActorShared80134f60* arg0);

#endif
