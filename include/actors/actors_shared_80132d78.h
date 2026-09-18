#ifndef ACTORS_SHARED_80132D78_H
#define ACTORS_SHARED_80132D78_H

#include "common.h"

/// One 0x10-byte entry of the box table `ActorShared80132d78Work::field_6B4`:
/// the entry's kind at `field_0`, and the box an actor occupies, x from
/// `field_8` to `field_C` and z from `field_E` to `field_A`.
typedef struct ActorShared80132d78Region {
    /* 0x0 */ s16 field_0;
    /* 0x2 */ s16 field_2;
    /* 0x4 */ s16 field_4;
    /* 0x6 */ s16 field_6;
    /* 0x8 */ s16 field_8;
    /* 0xA */ s16 field_A;
    /* 0xC */ s16 field_C;
    /* 0xE */ s16 field_E;
} ActorShared80132d78Region;
STATIC_ASSERT_SIZEOF(ActorShared80132d78Region, 0x10);

/// Work block the carriers hang off their context's 0x1C slot (the task's
/// `Task::work`, which is not a `TaskIdMap` here) -- the same block
/// `Actor402200Work` describes, seen through the three fields this body
/// reaches.
///
/// `field_6FA` is the box table's entry count, read as a signed halfword: a
/// non-positive count disarms the scan. `field_708` takes the index of the box
/// the scan reported a hit on.
typedef struct ActorShared80132d78Work {
    /* 0x000 */ byte                       pad_0[0x6B4];
    /* 0x6B4 */ ActorShared80132d78Region* field_6B4;
    /* 0x6B8 */ byte                       pad_6B8[0x42];
    /* 0x6FA */ s16                        field_6FA;
    /* 0x6FC */ byte                       pad_6FC[0xC];
    /* 0x708 */ s16                        field_708;
    /* 0x70A */ byte                       pad_70A[2];
} ActorShared80132d78Work;
STATIC_ASSERT_SIZEOF(ActorShared80132d78Work, 0x70C);

/// Actor context handed to this body: `field_1C` is the work block above.
typedef struct ActorShared80132d78 {
    /* 0x00 */ byte                     pad_0[0x1C];
    /* 0x1C */ ActorShared80132d78Work* field_1C;
} ActorShared80132d78;

/// Reports whether the player is standing inside one of the actor's boxes:
/// walks the `field_6FA`-entry table at `field_6B4` and, on the first entry
/// whose `field_0` is 1 and whose box holds the player's world position
/// (`Player_Status.coordMtx`, x between `field_8` and `field_C`, z between
/// `field_E` and `field_A`), parks that entry's index in `field_708` and
/// answers 1. A non-positive entry count, or no entry holding the position,
/// answers 0.
///
/// Shared verbatim by `actor_402200` and `actor_403900`.
s32 ActorsShared80132d78(ActorShared80132d78* arg0);

#endif
