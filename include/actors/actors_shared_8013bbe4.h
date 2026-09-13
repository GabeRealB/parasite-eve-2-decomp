#ifndef ACTORS_SHARED_8013BBE4_H
#define ACTORS_SHARED_8013BBE4_H

#include "common.h"

#include <psyq/libgte.h>
#include <psyq/libgpu.h>
#include <psyq/libgs.h>

/// The actor's pair of `GsCOORDINATE2`s, 0x50 apart - the shape every actor in
/// this family carries behind `field_2C->field_8`.
typedef struct ActorShared8013bbe4Coord {
    /* 0x00 */ GsCOORDINATE2 field_0;
    /* 0x50 */ GsCOORDINATE2 field_50;
} ActorShared8013bbe4Coord;
STATIC_ASSERT_SIZEOF(ActorShared8013bbe4Coord, 0xA0);

typedef struct ActorShared8013bbe4Obj2C {
    /* 0x00 */ byte                      pad_0[8];
    /* 0x08 */ ActorShared8013bbe4Coord* field_8;
} ActorShared8013bbe4Obj2C;

typedef struct ActorShared8013bbe4 {
    /* 0x00 */ byte                      pad_0[0x2C];
    /* 0x2C */ ActorShared8013bbe4Obj2C* field_2C;
} ActorShared8013bbe4;

/// Draws the actor's ground shadow: a quad at the *second* attach coordinate's
/// x/z and the *first*'s y, so the quad sits on the ground under the actor even
/// when the two parts are tilted apart.
///
/// Shared verbatim by `actor_510900` and `actor_521100`.
void ActorsShared8013bbe4(ActorShared8013bbe4* arg0);

#endif
