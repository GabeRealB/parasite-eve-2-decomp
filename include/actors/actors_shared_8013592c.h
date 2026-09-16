#ifndef ACTORS_SHARED_8013592C_H
#define ACTORS_SHARED_8013592C_H

#include "common.h"

/// Work block the carriers hang off their context's 0x1C slot -- the same
/// block `Actor402200Work` describes, seen through the fields this body
/// reaches.
typedef struct ActorShared8013592cWork {
    /* 0x000 */ byte pad_0[0x6C0];
    /* 0x6C0 */ s16  field_6C0; // animation id
    /* 0x6C2 */ byte pad_6C2[2];
    /* 0x6C4 */ s16  field_6C4; // animation frame counter
    /* 0x6C6 */ byte pad_6C6[6];
    /* 0x6CC */ s16  field_6CC;
    /* 0x6CE */ s16  field_6CE; // state
    /* 0x6D0 */ byte pad_6D0[4];
    /* 0x6D4 */ u16  field_6D4; // cue countdown
    /* 0x6D6 */ byte pad_6D6[0x1A];
    /* 0x6F0 */ s16  field_6F0;
    /* 0x6F2 */ byte pad_6F2[0x2A];
} ActorShared8013592cWork;
STATIC_ASSERT_SIZEOF(ActorShared8013592cWork, 0x71C);

/// Gameplay's LCG state, advanced as `state * 5 + 0x71357911`.
extern u32 Gp_LcgState;

/// Actor context handed to this body: `field_1C` is the work block above.
typedef struct ActorShared8013592c {
    /* 0x00 */ byte                     pad_0[0x1C];
    /* 0x1C */ ActorShared8013592cWork* field_1C;
} ActorShared8013592c;

/// Steps the actor through its opening animation: state 0 picks animation
/// 0xE or 0x12 from `field_6F0` and moves to state 1 or 2; those wait for the
/// frame counter to reach 0x10 or 0x16, then switch to animation 0x10 or 0x14,
/// enter state 3 and arm the `field_6D4` countdown from the `Gp_LcgState` LCG
/// (0..0x3F).
///
/// Shared verbatim by `actor_402200` and `actor_403900`.
void ActorsShared8013592c(ActorShared8013592c* arg0);

#endif
