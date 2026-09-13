#ifndef ACTORS_SHARED_80136C4C_H
#define ACTORS_SHARED_80136C4C_H

#include "common.h"

#include "main/task.h"

/// Work block of the enemy actors that share `ActorsShared80136c4c`, reached
/// through `Task::idMap`. Only the animation selector, the frame counter, the
/// dwell pair and the state fields are modelled here; the same offsets sit in
/// `Actor02000Work` (`include/actors/actor_102000.h`), which describes the rest
/// of the block.
typedef struct ActorsShared80136c4cWork {
    /* 0x000 */ byte pad_0[0x694];
    /* 0x694 */ s16  field_694; ///< animation to select
    /* 0x696 */ byte pad_696[2];
    /* 0x698 */ s16  field_698; ///< frames spent in the current animation
    /* 0x69A */ byte pad_69A[2];
    /* 0x69C */ s16  field_69C; ///< dwell counter, cleared on state 0 entry
    /* 0x69E */ s16  field_69E; ///< dwell counter, cleared on state 0 entry
    /* 0x6A0 */ byte pad_6A0[6];
    /* 0x6A6 */ s16  field_6A6; ///< animation to park
    /* 0x6A8 */ s16  field_6A8; ///< state
    /* 0x6AA */ s16  field_6AA; ///< animation the state-0 branch picks
} ActorsShared80136c4cWork;

/// State advance for the "hurt" clip. State 0 picks animation 0x12 and stays in
/// state 1 when `field_6AA` is 1, otherwise animation 0x13 and state 2, and
/// clears the dwell counters either way; state 1 waits for `field_698` to reach
/// 0x50 and state 2 for 0x3B, then both park animation 2 in `field_6A6` and
/// drop back to state 0.
void ActorsShared80136c4c(Task* task);

#endif
