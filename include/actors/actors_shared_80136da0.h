#ifndef ACTORS_SHARED_80136DA0_H
#define ACTORS_SHARED_80136DA0_H

#include "common.h"

#include "main/task.h"

/// Work block of the enemy actors that share `ActorsShared80136da0`, reached
/// through `Task::idMap`. Only the animation selector, the frame counter, the
/// state field and the state-0 branch selector are modelled here; the same
/// offsets sit in `Actor105700Work` (`include/actors/actor_105700.h`), which
/// describes the rest of the block.
typedef struct ActorsShared80136da0Work {
    /* 0x000 */ byte pad_0[0x694];
    /* 0x694 */ s16  field_694; ///< animation to select
    /* 0x696 */ byte pad_696[2];
    /* 0x698 */ s16  field_698; ///< frames spent in the current animation
    /* 0x69A */ byte pad_69A[0xE];
    /* 0x6A8 */ s16  field_6A8; ///< state
    /* 0x6AA */ byte pad_6AA[0xE];
    /// State-0 branch selector: 1 picks the 0x17 dwell, anything else the
    /// 0x1B one.
    /* 0x6B8 */ s16 field_6B8;
} ActorsShared80136da0Work;

/// Approach-cycle state advance. State 0 picks its animation off
/// `field_6B8` - 0x17 and a step back to the same value when it is 1, 0x1B
/// and step 2 otherwise; states 1 and 2 wait for `field_698` to reach 0x10
/// and 0x16, then hand the task over to the next handler (state 2) and drop
/// back to state 0.
void ActorsShared80136da0(Task* task);

#endif
