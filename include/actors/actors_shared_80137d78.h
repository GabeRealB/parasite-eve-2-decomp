#ifndef ACTORS_SHARED_80137D78_H
#define ACTORS_SHARED_80137D78_H

#include "common.h"

/// Work block the carriers hang off their context's 0x1C slot (the task's
/// `Task::idMap`, which is not a `TaskIdMap` here) -- the same block
/// `Actor402200Work` describes, seen through the fields this body reaches.
///
/// `field_6CE` is the state this body advances: 0 picks between two entrance
/// animations on `field_6F0` and re-parks the state on itself when that flag is
/// 1, and 1 / 2 each wait out a `field_6C4` threshold before dropping back to
/// 0. `field_6C0` is the animation id the state-0 branch selects and the
/// `field_6DA` / `field_6DC` / `field_6DE` trio the timers it arms alongside it.
///
/// The block is the dispatcher `ActorsShared80137b78`'s own work block - it
/// picks this body out of its `field_6CC` state at 0x6CC, two bytes below
/// `field_6CE` - so both views describe the same allocation.
typedef struct ActorShared80137d78Work {
    /* 0x000 */ byte pad_0[0x6C0];
    /* 0x6C0 */ s16  field_6C0; // entrance animation id
    /* 0x6C2 */ byte pad_6C2[2];
    /* 0x6C4 */ s16  field_6C4; // frames the current animation has ticked
    /* 0x6C6 */ byte pad_6C6[8];
    /* 0x6CE */ s16  field_6CE; // state this body advances
    /* 0x6D0 */ byte pad_6D0[0xA];
    /* 0x6DA */ s16  field_6DA;
    /* 0x6DC */ s16  field_6DC;
    /* 0x6DE */ s16  field_6DE;
    /* 0x6E0 */ byte pad_6E0[0x10];
    /* 0x6F0 */ s16  field_6F0; // 1 keeps the actor in the entrance
    /* 0x6F2 */ byte pad_6F2[0x2A];
} ActorShared80137d78Work;
STATIC_ASSERT_SIZEOF(ActorShared80137d78Work, 0x71C);

/// Actor context handed to this body: `field_1C` is the work block above and
/// `field_30` the state the actor is parked in for the frame dispatcher. Same
/// shape as the other actor overlays' contexts.
typedef struct ActorShared80137d78 {
    /* 0x00 */ byte                     pad_0[0x1C];
    /* 0x1C */ ActorShared80137d78Work* field_1C;
    /* 0x20 */ byte                     pad_20[0x10];
    /* 0x30 */ s32                      field_30;
} ActorShared80137d78;

/// State machine driving the actor's opening beat. State 0 branches on
/// `field_6F0`: while it is 1 the entrance animation 0xE is kept and the state
/// is left on 0, so the actor stays in the entrance until something else clears
/// the flag; otherwise the animation is set to 0x12 and the state to 2. Either
/// way the `field_6DA` / `field_6DC` / `field_6DE` timers are armed to 1, 0xA
/// and 5. States 1 and 2 each wait for `field_6C4` to reach their own threshold
/// (0x10 and 0x16) and then park the actor's `field_30` - state 2 with the
/// state it was in - and drop the state back to 0.
///
/// Shared verbatim by `actor_402200` and `actor_403900`.
void ActorsShared80137d78(ActorShared80137d78* arg0);

#endif
