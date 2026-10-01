#ifndef SRC_ACTORS_ACTOR_300700_ACTOR_300700_SPAWN2_PRIVATE_H
#define SRC_ACTORS_ACTOR_300700_ACTOR_300700_SPAWN2_PRIVATE_H

#include <psyq/sys/types.h>
#include <psyq/libgte.h>

#include "types.h"

#include "gameplay/actor.h"
#include "gameplay/animation.h"

#include "main/coord.h"
#include "main/session_types.h"

/// The 0x39C-byte allocation `ratSpawn` makes with
/// `memCalloc` and stores in `Task::work`, then fills with the four `WorldCollisionBody`
/// render nodes (`Gp_LinkObj`, shapes 3/2/2/3) and their `WorldCollisionContact` tables.
///
/// This is the work block the overlay's second enemy variant runs on - the
/// state table `gRatStateHandlers` (`ratUpdate` and
/// friends), which reaches it as `Actor300700Work` and only ever touches the
/// fields from 0x37A up. Those trailing fields, plus the animation context
/// `func_800B3F84` fills in, are laid out identically in both views; the
/// 0x1DC..0x333 run - the four list nodes and their record tables - is owned
/// only here, so `Actor300700Work` carries it as padding.
typedef struct Actor300700Spawn2Work {
    /* 0x000 */ AnimationContext      anim;
    /* 0x014 */ AnimationSlot         slots[7];
    /* 0x12C */ byte                  field_12C[0x50]; // pose buffer, func_800B3F84 arg3
    /* 0x17C */ MATRIX                field_17C;
    /* 0x19C */ MATRIX                field_19C;       // TmdObject color matrix
    /* 0x1BC */ MATRIX                field_1BC;       // TmdObject light matrix
    /* 0x1DC */ WorldCollisionBody    obj1;
    /* 0x1FC */ WorldCollisionContact rec1[1];
    /* 0x214 */ WorldCollisionBody    obj2;
    /* 0x234 */ WorldCollisionContact rec2[3];
    /* 0x27C */ WorldCollisionBody    obj3;
    /* 0x29C */ WorldCollisionContact rec3[4];
    /* 0x2FC */ WorldCollisionBody    obj4;
    /* 0x31C */ WorldCollisionContact rec4[1];
    /* 0x334 */ GfxCoord*             field_334;
    /* 0x338 */ s16                   field_338;
    /* 0x33A */ s16                   field_33A;
    /* 0x33C */ byte                  pad_33C[0x42];
    /* 0x37E */ s16                   field_37E; // current state id, `Actor300700Work`
    /* 0x380 */ s16                   field_380; // last applied state id
} Actor300700Spawn2Work;

#endif // SRC_ACTORS_ACTOR_300700_ACTOR_300700_SPAWN2_PRIVATE_H
